import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { spawnSync } from "node:child_process";

const root = path.resolve(import.meta.dirname, "../..");
const executable = path.join(root, "x64", "Debug", "LuaSharp.exe");
assert.ok(fs.existsSync(executable), `LuaSharp executable was not found at ${executable}`);

const directory = fs.mkdtempSync(path.join(os.tmpdir(), "luasharp-cli-"));
try {
    const valid = path.join(directory, "valid.ls");
    const generated = path.join(directory, "valid.generated.c");
    fs.writeFileSync(valid, 'class Warning { int run() { return 1; int dead = 2; } }\nint answer = 40 + 2;\nprintf("answer=%d\\n", answer);\n');
    const run = spawnSync(executable, [valid, "-o", generated], { encoding: "utf8" });
    assert.equal(run.status, 0, `native CLI failed:\n${run.stderr}`);
    assert.match(run.stdout, /answer=42/);
    assert.match(run.stdout, /Wrote .*valid\.generated\.c/);
    assert.match(run.stdout, /Ran .*valid\.ls/);
    assert.match(run.stdout, /Compiled in \d+\.\d ms \(frontend \d+\.\d ms, write C \d+\.\d ms, tcc \d+\.\d ms\)[\s\S]*answer=42/, "compile time must print before the program runs");
    assert.match(run.stderr, /warning: Unreachable code/);
    assert.ok(fs.readFileSync(generated, "utf8").includes("int ls_module_init(void)"), "CLI did not emit generated C");

    const invalid = path.join(directory, "invalid.ls");
    const invalidOutput = path.join(directory, "invalid.c");
    fs.writeFileSync(invalid, 'int value = "wrong";\n');
    const failed = spawnSync(executable, [invalid, "-o", invalidOutput], { encoding: "utf8" });
    assert.equal(failed.status, 1, "CLI accepted a semantic error");
    assert.match(failed.stderr, /invalid\.ls\(1,\d+\): error: Cannot assign/);
    assert.equal(fs.existsSync(invalidOutput), false, "CLI emitted C despite semantic errors");

    console.log("PASS LuaSharp CLI: diagnostics, C emission, native compilation, execution, and printf");
} finally {
    fs.rmSync(directory, { recursive: true, force: true });
}
