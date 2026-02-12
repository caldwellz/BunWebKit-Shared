//@ skip if $addressBits <= 32
//@ runDefaultWasm("-m", "--useWasmMemory64=1", "--useOMGJIT=0")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
    (module
        (memory (export "memory") i64 10 50)
        (func (export "write") (param $val i32) (param $addr i64)
            (i32.store (local.get $addr) (local.get $val))
        )
        (func (export "read") (param $addr i64) (result i32)
            (i32.load (local.get $addr))
        )
    )`;

const instance = await instantiate(wat, {}, {reference_types: true});
const {write, read} = instance.exports;

const writeAddr = BigInt(Number.MAX_SAFE_INTEGER + 1);
function test() {
    const writeAndRead = (val) => {
        write(val, writeAddr);

        const readResult = read(writeAddr);
        assert.eq(readResult, val);
    }

    writeAndRead(42);
    // reset
    writeAndRead(0);
}

for (let i = 0; i < wasmTestLoopCount; i++)
    test();
