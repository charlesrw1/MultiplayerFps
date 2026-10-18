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

    // check and emit-c see the declarations of luasharp.json; a missing declaration path is a warning.
    const project = path.join(directory, "project");
    fs.mkdirSync(path.join(project, ".luasharp"), { recursive: true });
    fs.mkdirSync(path.join(project, "game"));
    fs.writeFileSync(path.join(project, "luasharp.json"), JSON.stringify({ declarations: [".luasharp", "missing.ls"] }));
    fs.writeFileSync(path.join(project, ".luasharp", "engine.ls"), "extern class Engine { static int Frame(); }");
    const user = path.join(project, "game", "user.ls");
    fs.writeFileSync(user, "int frame = Engine.Frame();\n");
    const checked = spawnSync(executable, ["check", user], { encoding: "utf8" });
    assert.equal(checked.status, 0, `check did not use the project declarations:\n${checked.stderr}`);
    assert.match(checked.stderr, /declaration path 'missing\.ls' does not exist/);
    const outside = path.join(directory, "outside.ls");
    fs.copyFileSync(user, outside);
    assert.equal(spawnSync(executable, ["check", outside], { encoding: "utf8" }).status, 1, "a file outside the project saw its declarations");
    const plain = path.join(project, "game", "plain.ls");
    fs.writeFileSync(plain, 'printf("plain=%d\\n", 7);\n');
    const plainRun = spawnSync(executable, ["run", plain], { encoding: "utf8" });
    assert.equal(plainRun.status, 0, `run in a project must not link the host declarations:\n${plainRun.stderr}`);
    assert.match(plainRun.stdout, /plain=7/);

    // Several files are one program: each sees the classes of the others.
    const main = path.join(directory, "main.ls");
    const helper = path.join(directory, "helper.ls");
    fs.writeFileSync(main, 'printf("sum=%d\\n", Helper.add(2, 3));\n');
    fs.writeFileSync(helper, "class Helper { static int add(int a, int b) { return a + b; } }\nint broken = \"text\";\n");
    const brokenProgram = spawnSync(executable, ["check", main, helper], { encoding: "utf8" });
    assert.equal(brokenProgram.status, 1, "check accepted an error in the second file");
    assert.match(brokenProgram.stderr, /helper\.ls\(2,\d+\): error: Cannot assign/);
    fs.writeFileSync(helper, "class Helper { static int add(int a, int b) { return a + b; } }\n");
    const program = spawnSync(executable, ["run", main, helper, "-o", path.join(directory, "program.c")], { encoding: "utf8" });
    assert.equal(program.status, 0, `a file could not use a class of another file:\n${program.stderr}`);
    assert.match(program.stdout, /sum=5/);

    console.log("PASS LuaSharp CLI: diagnostics, C emission, native compilation, execution, printf, multi-file programs, and luasharp.json projects");
} finally {
    fs.rmSync(directory, { recursive: true, force: true });
}
