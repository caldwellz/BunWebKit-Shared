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

# WebKit/JavaScriptCore requires ICU. We build ICU statically using MSBuild
# with /MT (static CRT) to avoid shipping DLLs with Bun.
# Note: This approach replaces the previous Cygwin-based autotools build.
# The makedata project requires Python 3 to generate the ICU data.
$ICU_SOURCE_URL = "https://github.com/unicode-org/icu/releases/download/release-73-2/icu4c-73_2-src.tgz"

# Verify Python 3 is available (required for ICU data build)
try {
    $pythonVersion = & py -3 --version 2>&1
    Write-Host ":: Found Python: $pythonVersion"
} catch {
    throw "Python 3 is required to build ICU data. Please install Python 3 and ensure 'py -3' is available."
}

$ICU_STATIC_ROOT = Join-Path $WebKitBuild "icu"
$ICU_STATIC_LIBRARY = Join-Path $ICU_STATIC_ROOT "lib"
$ICU_STATIC_INCLUDE_DIR = Join-Path $ICU_STATIC_ROOT "include"

$null = mkdir $WebKitBuild -ErrorAction SilentlyContinue

# Function to patch vcxproj files for static library build with /MT
function Patch-IcuVcxProj {
    param(
        [string]$FilePath,
        [string]$Configuration
    )

    $BackupPath = "$FilePath.bak"

    if (-not (Test-Path $FilePath)) {
        throw "File not found: $FilePath"
    }

    # Create backup if not exists
    if (-not (Test-Path $BackupPath)) {
        Copy-Item $FilePath $BackupPath
        Write-Host "  Backed up: $(Split-Path -Leaf $FilePath)"
    }

    $content = Get-Content $FilePath -Raw

    # 1. Change ConfigurationType from DynamicLibrary to StaticLibrary
    $content = $content -replace '<ConfigurationType>DynamicLibrary</ConfigurationType>', '<ConfigurationType>StaticLibrary</ConfigurationType>'

    # 2. Change RuntimeLibrary for Release: MultiThreadedDLL -> MultiThreaded
    $content = $content -replace '<RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>', '<RuntimeLibrary>MultiThreaded</RuntimeLibrary>'

    # 3. Change RuntimeLibrary for Debug: MultiThreadedDebugDLL -> MultiThreadedDebug
    $content = $content -replace '<RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary>', '<RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>'

    # 4. (now skipped) Add U_STATIC_IMPLEMENTATION to preprocessor definitions

    # 5. Disable Whole Program Optimization (/GL) - required for lld-link compatibility
    # MSBuild with /GL generates LTCG object files that only work with MSVC's link.exe
    # Since JSC uses clang-cl with lld-link, we must disable /GL
    $content = $content -replace '<WholeProgramOptimization>true</WholeProgramOptimization>', '<WholeProgramOptimization>false</WholeProgramOptimization>'

    # 6. Remove DLL-specific link settings
    $content = $content -replace '<OutputFile>[^<]*\.(dll|DLL)</OutputFile>', ''
    $content = $content -replace '<ImportLibrary>[^<]*</ImportLibrary>', ''

    # For stubdata - remove resource-only DLL settings
    if ($FilePath -match "stubdata") {
        $content = $content -replace '<NoEntryPoint>true</NoEntryPoint>', ''
        $content = $content -replace '<TurnOffAssemblyGeneration>true</TurnOffAssemblyGeneration>', ''
    }

    Set-Content $FilePath $content -NoNewline
    Write-Host "  Patched: $(Split-Path -Leaf $FilePath)"
}

if (!(Test-Path -Path $ICU_STATIC_ROOT) -or !(Test-Path -Path "$ICU_STATIC_LIBRARY/sicudt.lib")) {
    $ICU_STATIC_TAR = Join-Path $WebKitBuild "icu4c-src.tgz"

    if (!(Test-Path $ICU_STATIC_TAR)) {
        Write-Host ":: Downloading ICU"
        Invoke-WebRequest -Uri $ICU_SOURCE_URL -OutFile $ICU_STATIC_TAR
    }

    if (!(Test-Path "$ICU_STATIC_ROOT/source")) {
        Write-Host ":: Extracting ICU"
        tar.exe -xzf $ICU_STATIC_TAR -C $WebKitBuild
        if ($LASTEXITCODE -ne 0) { throw "tar failed with exit code $LASTEXITCODE" }
    }

    $IcuSourceDir = Join-Path $ICU_STATIC_ROOT "source"

    # Patch makedata.mak to skip DLL copy when building static
    # The original makedata.mak always tries to copy icudt*.dll even when -m static is used
    # We use -ignore flag with copy to suppress errors when the DLL doesn't exist (static build)
    $makedataMak = Join-Path $IcuSourceDir "data\makedata.mak"
    if (Test-Path $makedataMak) {
        Write-Host ":: Patching makedata.mak to skip DLL copy for static build..."
        $makedataContent = Get-Content $makedataMak -Raw

        # Add '-' prefix to copy command to ignore errors (nmake ignores exit code with - prefix)
        # This makes the copy optional - it succeeds if file exists, silently continues if not
        $makedataContent = $makedataContent -replace '\tcopy "\$\(U_ICUDATA_NAME\)\.dll"', "`t-copy `"`$(U_ICUDATA_NAME).dll`""

        Set-Content $makedataMak $makedataContent -NoNewline
        Write-Host "  Patched: makedata.mak"
    }

    # Find MSBuild
    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere not found. Please install Visual Studio."
    }

    $vsPath = & $vswhere -latest -property installationPath
    $msbuildPath = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"

    if (-not (Test-Path $msbuildPath)) {
        throw "MSBuild not found at: $msbuildPath"
    }

    Write-Host ""
    Write-Host ":: Using MSBuild: $msbuildPath"

    $slnPath = Join-Path $IcuSourceDir "allinone\allinone.sln"
    $msbuildConfiguration = $CMAKE_BUILD_TYPE

    # ========================================================================
    # STAGE 1: Build makedata with default DLL configuration
    # ========================================================================
    # The ICU tools (pkgdata, etc.) require DLL linkage to build and run.
    # We build makedata first with default configuration to generate ICU data.
    # The output sicudt.lib is pure data with no CRT dependencies, so it works
    # with any runtime configuration.
    Write-Host ""
    Write-Host ":: STAGE 1: Building ICU makedata (generates ICU data)..."

    # Set ICU_PACKAGE_MODE to build static data library instead of DLL
    # This is passed to makedata.mak via environment variable
    $env:ICU_PACKAGE_MODE = "-m static"
    Write-Host ":: ICU_PACKAGE_MODE set to: $env:ICU_PACKAGE_MODE"

    $buildArgs = @(
        $slnPath,
        "/t:makedata",
        "/p:Configuration=$msbuildConfiguration",
        "/p:Platform=$Platform",
        "/p:SkipUWP=true",
        "/p:WindowsTargetPlatformVersion=10.0",
        "/m",
        "/v:normal"
    )

    & $msbuildPath $buildArgs

    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed for makedata with exit code $LASTEXITCODE"
    }

    Write-Host ":: Built makedata successfully"

    # ========================================================================
    # STAGE 2: Rebuild common and i18n as static libraries with /MT
    # ========================================================================
    # Now that we have the ICU data, we patch and rebuild the core libraries
    # with static CRT (/MT) for use in Bun.
    Write-Host ""
    Write-Host ":: STAGE 2: Rebuilding ICU common and i18n as static libraries with /MT..."

    # Patch vcxproj files for static build with /MT
    Write-Host ":: Patching ICU vcxproj files for static build..."
    $vcxprojFiles = @(
        "common\common.vcxproj",
        "i18n\i18n.vcxproj",
        "stubdata\stubdata.vcxproj"
    )

    foreach ($file in $vcxprojFiles) {
        Patch-IcuVcxProj -FilePath (Join-Path $IcuSourceDir $file) -Configuration $CMAKE_BUILD_TYPE
    }

    # Patch the .props file to disable /GL (Whole Program Optimization)
    # This is required because lld-link cannot read LTCG object files
    $propsFile = Join-Path $IcuSourceDir "allinone\Build.Windows.ProjectConfiguration.props"
    if (Test-Path $propsFile) {
        Write-Host ":: Patching Build.Windows.ProjectConfiguration.props to disable /GL..."
        $propsContent = Get-Content $propsFile -Raw
        $propsContent = $propsContent -replace '<WholeProgramOptimization>true</WholeProgramOptimization>', '<WholeProgramOptimization>false</WholeProgramOptimization>'
        $propsContent = $propsContent -replace '<LinkTimeCodeGeneration>UseLinkTimeCodeGeneration</LinkTimeCodeGeneration>', '<LinkTimeCodeGeneration></LinkTimeCodeGeneration>'
        Set-Content $propsFile $propsContent -NoNewline
        Write-Host "  Patched: Build.Windows.ProjectConfiguration.props"
    }

    # Rebuild common and i18n with /MT (they need full rebuild after patching)
    foreach ($target in @("common", "i18n")) {
        Write-Host ":: Building ICU $target with /MT..."

        $buildArgs = @(
            $slnPath,
            "/t:$target",
            "/p:Configuration=$msbuildConfiguration",
            "/p:Platform=$Platform",
            "/p:SkipUWP=true",
            "/p:WindowsTargetPlatformVersion=10.0",
            "/m",
            "/v:minimal"
        )

        & $msbuildPath $buildArgs

        if ($LASTEXITCODE -ne 0) {
            throw "MSBuild failed for $target with exit code $LASTEXITCODE"
        }

        Write-Host ":: Built $target successfully"
    }

    # Copy output files to expected locations
    Write-Host ""
    Write-Host ":: Copying ICU output files..."

    $null = mkdir -Force $ICU_STATIC_INCLUDE_DIR/unicode
    $null = mkdir -Force $ICU_STATIC_LIBRARY

    # Copy headers
    Copy-Item -r "$IcuSourceDir/common/unicode/*" "$ICU_STATIC_INCLUDE_DIR/unicode"
    Copy-Item -r "$IcuSourceDir/i18n/unicode/*" "$ICU_STATIC_INCLUDE_DIR/unicode"

    # Find and copy static libraries from MSBuild output
    # MSBuild outputs to: <project>/<Platform>/<Configuration>/<project>.lib
    $commonLibSrc = Join-Path $IcuSourceDir "common\$Platform\$msbuildConfiguration\common.lib"
    $i18nLibSrc = Join-Path $IcuSourceDir "i18n\$Platform\$msbuildConfiguration\i18n.lib"

    # The ICU data library is generated by makedata with pkgdata tool
    # Output location depends on platform: bin64 for x64, binARM64 for ARM64
    $binDir = if ($Platform -eq "x64") { "bin64" } else { "bin$Platform" }

    # pkgdata generates sicudt73.lib (static ICU data) when ICU_PACKAGE_MODE=-m static
    $icuDataLibSrc = Join-Path $IcuSourceDir "..\$binDir\sicudt73.lib"

    # Also check alternative locations
    if (-not (Test-Path $icuDataLibSrc)) {
        $icuDataLibSrc = Join-Path $IcuSourceDir "data\out\tmp\sicudt73.lib"
    }
    if (-not (Test-Path $icuDataLibSrc)) {
        $icuDataLibSrc = Join-Path $IcuSourceDir "data\out\sicudt73.lib"
    }
    if (-not (Test-Path $icuDataLibSrc)) {
        # Search for the file in the ICU directory tree
        Write-Host ":: Searching for ICU data library..."
        $foundLib = Get-ChildItem -Path $ICU_STATIC_ROOT -Recurse -Filter "sicudt*.lib" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($foundLib) {
            $icuDataLibSrc = $foundLib.FullName
            Write-Host ":: Found ICU data library at: $icuDataLibSrc"
        }
    }

    # Copy common library
    if (Test-Path $commonLibSrc) {
        Copy-Item $commonLibSrc "$ICU_STATIC_LIBRARY/icuuc.lib" -Force
        Write-Host "  Copied: common.lib -> icuuc.lib"
    } else {
        throw "ICU common library not found at: $commonLibSrc"
    }

    # Copy i18n library
    if (Test-Path $i18nLibSrc) {
        Copy-Item $i18nLibSrc "$ICU_STATIC_LIBRARY/icuin.lib" -Force
        Write-Host "  Copied: i18n.lib -> icuin.lib"
    } else {
        throw "ICU i18n library not found at: $i18nLibSrc"
    }

    # Copy data library (with full Unicode data, not just stub)
    if (Test-Path $icuDataLibSrc) {
        Copy-Item $icuDataLibSrc "$ICU_STATIC_LIBRARY/sicudt.lib" -Force
        Write-Host "  Copied: $(Split-Path -Leaf $icuDataLibSrc) -> sicudt.lib"
    } else {
        # List what files were actually generated for debugging
        Write-Host ":: WARNING: ICU data library not found. Listing generated files..."
        Get-ChildItem -Path $ICU_STATIC_ROOT -Recurse -Filter "*.lib" | ForEach-Object {
            Write-Host "    Found: $($_.FullName)"
        }
        throw "ICU data library not found. Expected at: $icuDataLibSrc"
    }

    Write-Host ":: ICU build completed successfully!"
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

$NoWebassembly = if ($env:NO_WEBASSEMBLY) { $env:NO_WEBASSEMBLY } else { $false }
$WebAssemblyState = if ($NoWebassembly) { "OFF" } else { "ON" }

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
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}" `
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
    "-DICU_ROOT=${ICU_STATIC_ROOT}" `
    "-DICU_LIBRARY=${ICU_STATIC_LIBRARY}" `
    "-DICU_INCLUDE_DIR=${ICU_STATIC_INCLUDE_DIR}" `
    "-DCMAKE_C_COMPILER=${ClangPath}" `
    "-DCMAKE_CXX_COMPILER=${ClangPath}" `
    "-DCMAKE_LINKER=${LldLinkPath}" `
    -DCMAKE_C_FLAGS_RELEASE="/Zi /O2 /Ob2 /DNDEBUG /DJS_EXPORT_PRIVATE= ${ArchFlags} ${ARM64SehWorkaround}" `
    -DCMAKE_CXX_FLAGS_RELEASE="/Zi /O2 /Ob2 /DNDEBUG /DJS_EXPORT_PRIVATE= /clang:-fno-c++-static-destructors ${ArchFlags} ${ARM64SehWorkaround}" `
    -DCMAKE_C_FLAGS_DEBUG="/Zi /FS /O0 /Ob0 /DJS_EXPORT_PRIVATE= ${ArchFlags} ${ARM64SehWorkaround}" `
    -DCMAKE_CXX_FLAGS_DEBUG="/Zi /FS /O0 /Ob0 /DJS_EXPORT_PRIVATE= /clang:-fno-c++-static-destructors ${ArchFlags} ${ARM64SehWorkaround}" `
    "-DCMAKE_SHARED_LINKER_FLAGS=${env:LINKFLAGS}" `
    -DENABLE_REMOTE_INSPECTOR=OFF `
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CmakeMsvcRuntimeLibrary}" `
    -DBUILD_SHARED_LIBS=ON
    "-DLTO_MODE=${LTOMode}" `
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

# Copy ICU libraries to output with 's' prefix (static) naming convention
# This is the naming convention expected by Bun
# Note: sicudt.lib contains the full ICU Unicode data (built via makedata project)
if ($CMAKE_BUILD_TYPE -eq "Release") {
    Copy-Item "$ICU_STATIC_LIBRARY/sicudt.lib" "$output/lib/sicudt.lib" -Force
    Copy-Item "$ICU_STATIC_LIBRARY/icuin.lib" "$output/lib/sicuin.lib" -Force
    Copy-Item "$ICU_STATIC_LIBRARY/icuuc.lib" "$output/lib/sicuuc.lib" -Force
} elseif ($CMAKE_BUILD_TYPE -eq "Debug") {
    Copy-Item "$ICU_STATIC_LIBRARY/sicudt.lib" "$output/lib/sicudtd.lib" -Force
    Copy-Item "$ICU_STATIC_LIBRARY/icuin.lib" "$output/lib/sicuind.lib" -Force
    Copy-Item "$ICU_STATIC_LIBRARY/icuuc.lib" "$output/lib/sicuucd.lib" -Force
}

Add-Content -Path $output/include/cmakeconfig.h -Value "`#define BUN_WEBKIT_VERSION `"$BUN_WEBKIT_VERSION`""

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
Copy-Item -r $ICU_STATIC_INCLUDE_DIR/* $output/include/
# Note: ICU libraries are already copied with correct names above (sicudt.lib, sicuin.lib, sicuuc.lib)

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
