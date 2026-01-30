param(
    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64"
)
$ErrorActionPreference = "Stop"

# Set up MSVC environment variables. This is taken from Bun's 'scripts\env.ps1'
if ($env:VSINSTALLDIR -eq $null) {
    Write-Host "Loading Visual Studio environment, this may take a second..."
    $vsDir = Get-ChildItem -Path "C:\Program Files\Microsoft Visual Studio\2022" -Directory
    if ($vsDir -eq $null) {
        throw "Visual Studio directory not found."
    }
    Push-Location $vsDir
    try {
        $targetArch = if ($Platform -eq "ARM64") { "arm64" } else { "amd64" }
        . (Join-Path -Path $vsDir.FullName -ChildPath "Common7\Tools\Launch-VsDevShell.ps1") -Arch $targetArch -HostArch amd64
    }
    finally { Pop-Location }
}

if ($Platform -eq "x64" -and $Env:VSCMD_ARG_TGT_ARCH -eq "x86") {
    # Please do not try to compile Bun for 32 bit. It will not work. I promise.
    throw "Visual Studio environment is targetting 32 bit. This configuration is definetly a mistake."
}

# Fix up $PATH - remove mingw and strawberry perl paths that can interfere with MSVC
Write-Host $env:PATH

$SplitPath = $env:PATH -split ";";
$MSVCPaths = $SplitPath | Where-Object { $_ -like "*Microsoft Visual Studio*" }
$SplitPath = $MSVCPaths + ($SplitPath | Where-Object { $_ -notlike "*Microsoft Visual Studio*" } | Where-Object { $_ -notlike "*mingw*" })
$PathWithPerl = $SplitPath -join ";"
$env:PATH = ($SplitPath | Where-Object { $_ -notlike "*strawberry*" }) -join ';'

Write-Host $env:PATH

(Get-Command link).Path
clang-cl.exe --version

$env:CC = "clang-cl"
$env:CXX = "clang-cl"

$output = if ($env:WEBKIT_OUTPUT_DIR) { $env:WEBKIT_OUTPUT_DIR } else { "bun-webkit" }
$WebKitBuild = if ($env:WEBKIT_BUILD_DIR) { $env:WEBKIT_BUILD_DIR } else { "WebKitBuild" }
$CMAKE_BUILD_TYPE = if ($env:CMAKE_BUILD_TYPE) { $env:CMAKE_BUILD_TYPE } else { "Release" }
$BUN_WEBKIT_VERSION = if ($env:BUN_WEBKIT_VERSION) { $env:BUN_WEBKIT_VERSION } else { $(git rev-parse HEAD) }

$null = mkdir $WebKitBuild -ErrorAction SilentlyContinue

# WebKit/JavaScriptCore requires ICU, but unlike the Bun upstream, we can just bundle prebuilt DLLs.
$ICU_MAJOR_VERSION = "78"
$ICU_ROOT = Join-Path $WebKitBuild "icu"
$ICU_BIN_DIR = Join-Path $ICU_ROOT "bin64"
$ICU_LIB_DIR = Join-Path $ICU_ROOT "lib64"
$ICU_INCLUDE_DIR = Join-Path $ICU_ROOT "include"
if (!(Test-Path -Path $ICU_ROOT) -or !(Test-Path -Path "$ICU_LIB_DIR/icudt.lib")) {
    if ($Platform -eq "ARM64") {
        $ICUPlatform = "WinARM64"
    } else {
        $ICUPlatform = "Win64"
    }
    $ICU_ZIP = "icu4c-${ICU_MAJOR_VERSION}.1-${ICUPlatform}-MSVC2022.zip"
    $ICU_ZIP_PATH = Join-Path $WebKitBuild $ICU_ZIP
    $ICU_RELEASE_URL = "https://github.com/unicode-org/icu/releases/download/release-${ICU_MAJOR_VERSION}.1/${ICU_ZIP}"

    if (!(Test-Path $ICU_ZIP_PATH)) {
        Write-Host ":: Downloading ICU"
        Invoke-WebRequest -Uri $ICU_RELEASE_URL -OutFile $ICU_ZIP_PATH
    }

    if (!(Test-Path $ICU_LIB_DIR)) {
        Write-Host ":: Extracting ICU"
        unzip $ICU_ZIP_PATH -d $ICU_ROOT
        if ($LASTEXITCODE -ne 0) { throw "unzip failed with exit code $LASTEXITCODE" }
    }
}

Write-Host ":: Configuring WebKit"

$env:PATH = $PathWithPerl

$env:CFLAGS = "/Zi"
$env:CXXFLAGS = "/Zi"
$env:LINKFLAGS = "/FORCE:MULTIPLE"

$CmakeMsvcRuntimeLibrary = "MultiThreaded"
if ($CMAKE_BUILD_TYPE -eq "Debug") {
    $CmakeMsvcRuntimeLibrary = "MultiThreadedDebug"
}

# For ARM64, use the explicitly installed LLVM toolchain instead of VS's x64 LLVM
# The VS Developer Shell adds VS's x64 LLVM to PATH which would be used otherwise
if ($Platform -eq "ARM64") {
    $ArchFlags = "/clang:-march=armv8-a /clang:-mbranch-protection=standard /clang:-fstack-protector-strong"
    $ClangPath = "C:/LLVM/bin/clang-cl.exe"
    $LldLinkPath = "C:/LLVM/bin/lld-link.exe"
    # Note: The LLVM SEH unwind bug on Windows ARM64 (llvm/llvm-project#47432) is worked
    # around by disabling the probe functionality in MacroAssemblerARM64.cpp rather than
    # using compiler flags.
    $ARM64SehWorkaround = ""
    Write-Host ":: Using ARM64 LLVM toolchain: $ClangPath"
} else {
    $ArchFlags = "/clang:-march=x86-64-v3 /clang:-fcf-protection=full /clang:-fstack-protector-strong"
    $ClangPath = "clang-cl"
    $LldLinkPath = "lld-link"
    $ARM64SehWorkaround = ""
}

if ($CMAKE_BUILD_TYPE -eq "Debug") {
    $LTOMode = "OFF"
} else {
    $LTOMode = "thin"
}
cmake -S . -B $WebKitBuild `
    -DPORT="JSCOnly" `
    -DENABLE_STATIC_JSC=OFF `
    -DALLOW_LINE_AND_COLUMN_NUMBER_IN_BUILTINS=ON `
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" `
    -DUSE_THIN_ARCHIVES=OFF `
    -DENABLE_JAVASCRIPT_SHELL=OFF `
    -DENABLE_JIT=ON `
    -DENABLE_DFG_JIT=ON `
    -DENABLE_FTL_JIT=ON `
    -DENABLE_SAMPLING_PROFILER=OFF `
    -DENABLE_WEBASSEMBLY=ON `
    -DUSE_BUN_JSC_ADDITIONS=OFF `
    -DUSE_BUN_EVENT_LOOP=OFF `
    -DENABLE_BUN_SKIP_FAILING_ASSERTIONS=ON `
    -DICU_ROOT="${ICU_ROOT}" `
    -DICU_LIB_DIR="${ICU_LIB_DIR}" `
    -DICU_INCLUDE_DIR="${ICU_INCLUDE_DIR}" `
    -DCMAKE_C_COMPILER="${ClangPath}" `
    -DCMAKE_CXX_COMPILER="${ClangPath}" `
    -DCMAKE_LINKER="${LldLinkPath}" `
    -DCMAKE_C_FLAGS_RELEASE="/Zi /O2 /Ob2 /DNDEBUG /DJS_EXPORT_PRIVATE= ${ArchFlags} ${ARM64SehWorkaround}" `
    -DCMAKE_CXX_FLAGS_RELEASE="/Zi /O2 /Ob2 /DNDEBUG /DJS_EXPORT_PRIVATE= /clang:-fno-c++-static-destructors ${ArchFlags} ${ARM64SehWorkaround}" `
    -DCMAKE_C_FLAGS_DEBUG="/Zi /FS /O0 /Ob0 /DJS_EXPORT_PRIVATE= ${ArchFlags} ${ARM64SehWorkaround}" `
    -DCMAKE_CXX_FLAGS_DEBUG="/Zi /FS /O0 /Ob0 /DJS_EXPORT_PRIVATE= /clang:-fno-c++-static-destructors ${ArchFlags} ${ARM64SehWorkaround}" `
    -DCMAKE_SHARED_LINKER_FLAGS="${env:LINKFLAGS}" `
    -DENABLE_REMOTE_INSPECTOR=OFF `
    -DCMAKE_MSVC_RUNTIME_LIBRARY="${CmakeMsvcRuntimeLibrary}" `
    -DBUILD_SHARED_LIBS=ON `
    -DLTO_MODE="${LTOMode}" `
    -G Ninja
if ($LASTEXITCODE -ne 0) { throw "cmake failed with exit code $LASTEXITCODE" }

# Sometimes WasmOps.h isn't generated during the CMake configure step for whatever reason, so manually regenerate it
python Source\JavaScriptCore\wasm\generateWasmOpsHeader.py Source\JavaScriptCore\wasm\wasm.json WebKitBuild/JavaScriptCore/DerivedSources/WasmOps.h
if ($LASTEXITCODE -ne 0) { throw "python failed with exit code $LASTEXITCODE" }

# Workaround for what is probably a CMake bug
$batFiles = Get-ChildItem -Path $WebKitBuild -Filter "*.bat" -File -Recurse
foreach ($file in $batFiles) {
    $content = Get-Content $file.FullName -Raw
    $newContent = $content -replace "(\|\| \(set FAIL_LINE=\d+& goto :ABORT\))", ""
    if ($content -ne $newContent) {
        Set-Content -Path $file.FullName -Value $newContent
        Write-Host ":: Patch $($file.FullName)"
    }
}

Write-Host ":: Building WebKit"
cmake --build $WebKitBuild --config $CMAKE_BUILD_TYPE --verbose
if ($LASTEXITCODE -ne 0) { throw "cmake --build failed with exit code $LASTEXITCODE" }

Write-Host ":: Packaging ${output}"

# Dump the entire tree of files in $WebKitBuild to the console.
# This is useful for debugging.
Get-ChildItem -Recurse $WebKitBuild | Format-List -Force | Out-String | Write-Host

Remove-Item -Recurse -ErrorAction SilentlyContinue $output
$null = mkdir -ErrorAction SilentlyContinue $output
$null = mkdir -ErrorAction SilentlyContinue $output/include
$null = mkdir -ErrorAction SilentlyContinue $output/include/JavaScriptCore
$null = mkdir -ErrorAction SilentlyContinue $output/include/JavaScriptCore/internal
$null = mkdir -ErrorAction SilentlyContinue $output/include/wtf

Copy-Item -Recurse $WebKitBuild/lib $output
Copy-Item -Recurse $WebKitBuild/bin $output

# If there's a lib64, also copy it.
if (Test-Path -Path $WebKitBuild/lib64) {
    Copy-Item -Recurse $WebKitBuild/lib64/* $output/lib
}

Copy-Item $WebKitBuild/cmakeconfig.h $output/include/cmakeconfig.h
Add-Content -Path $output/include/cmakeconfig.h -Value "`#define BUN_WEBKIT_VERSION `"$BUN_WEBKIT_VERSION`""

# Copy ICU libs and DLLs to output
Copy-Item "$ICU_LIB_DIR/icudt.lib" "$output/lib/icudt.lib" -Force
Copy-Item "$ICU_LIB_DIR/icuin.lib" "$output/lib/icuin.lib" -Force
Copy-Item "$ICU_LIB_DIR/icuuc.lib" "$output/lib/icuuc.lib" -Force
Copy-Item "$ICU_BIN_DIR/icudt${ICU_MAJOR_VERSION}.dll" "$output/bin/icudt${ICU_MAJOR_VERSION}.dll" -Force
Copy-Item "$ICU_BIN_DIR/icuin${ICU_MAJOR_VERSION}.dll" "$output/bin/icuin${ICU_MAJOR_VERSION}.dll" -Force
Copy-Item "$ICU_BIN_DIR/icuuc${ICU_MAJOR_VERSION}.dll" "$output/bin/icuuc${ICU_MAJOR_VERSION}.dll" -Force

# Copy JSC headers
Copy-Item -r -Force $WebKitBuild/JavaScriptCore/DerivedSources/* $output/include/JavaScriptCore/internal/
Copy-Item -r -Force $WebKitBuild/JavaScriptCore/Headers/JavaScriptCore/* $output/include/JavaScriptCore/
Copy-Item -r -Force $WebKitBuild/JavaScriptCore/PrivateHeaders/JavaScriptCore/* $output/include/JavaScriptCore/internal/
# Recursively copy all the .h files in DerivedSources to the root of include/JavaScriptCore/internal, preserving the basename only.
Copy-Item -r -Force $WebKitBuild/JavaScriptCore/DerivedSources/*.h $output/include/JavaScriptCore/internal/
Copy-Item -r -Force $WebKitBuild/JavaScriptCore/DerivedSources/*/*.h $output/include/JavaScriptCore/internal/

# Recursively copy all the .json files in DerivedSources to the root of the output directory, preserving the basename only.
Copy-Item -r -Force $WebKitBuild/JavaScriptCore/DerivedSources/*.json $output/

# Copy-Item -r $WebKitBuild/WTF/DerivedSources/* $output/include/wtf/
Copy-Item -r $WebKitBuild/WTF/Headers/wtf/* $output/include/wtf/

# Copy bmalloc headers if they exist (libpas support)
if (Test-Path -Path $WebKitBuild/bmalloc) {
    $null = mkdir -ErrorAction SilentlyContinue $output/include/bmalloc
    Copy-Item -r $WebKitBuild/bmalloc/Headers/bmalloc/* $output/include/bmalloc/
}

(Get-Content -Path $output/include/JavaScriptCore/JSValueInternal.h) `
    -replace "#import <JavaScriptCore/JSValuePrivate.h>", "#include <JavaScriptCore/JSValuePrivate.h>" `
| Set-Content -Path $output/include/JavaScriptCore/JSValueInternal.h

# Copy ICU headers to output
Copy-Item -r $ICU_INCLUDE_DIR/* $output/include/
# Note: ICU libraries are already copied with correct names above (icudt.lib, icuin.lib, icuuc.lib)

$packageJsonContent = @{
    name       = $env:PACKAGE_JSON_LABEL
    version    = "0.0.1-$env:GITHUB_SHA"
    os         = @("windows")
    cpu        = @($env:PACKAGE_JSON_ARCH)
    repository = "https://github.com/$($env:GITHUB_REPOSITORY)"
} | ConvertTo-Json -Depth 2
Out-File -FilePath $output/package.json -InputObject $packageJsonContent

tar -cz -f "${output}.tar.gz" "${output}"
if ($LASTEXITCODE -ne 0) { throw "tar failed with exit code $LASTEXITCODE" }

Write-Host "-> ${output}.tar.gz"
