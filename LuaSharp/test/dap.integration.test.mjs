import assert from "node:assert/strict";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";
import { spawn } from "node:child_process";

const root = path.resolve(import.meta.dirname, "../..");
const executable = path.join(root, "x64", "Debug", "LuaSharp.exe");
assert.ok(fs.existsSync(executable), `LuaSharp executable was not found at ${executable}`);

// A minimal DAP client: requests by sequence, and a wait for any message.
class DapClient {
    constructor(socket) {
        this.socket = socket;
        this.sequence = 1;
        this.buffer = Buffer.alloc(0);
        this.messages = [];
        this.waiters = [];
        socket.on("data", (chunk) => this.receive(chunk));
    }
    receive(chunk) {
        this.buffer = Buffer.concat([this.buffer, chunk]);
        for (;;) {
            const headerEnd = this.buffer.indexOf("\r\n\r\n");
            if (headerEnd < 0) return;
            const length = Number(/Content-Length: (\d+)/.exec(this.buffer.subarray(0, headerEnd).toString())[1]);
            if (this.buffer.length < headerEnd + 4 + length) return;
            this.messages.push(JSON.parse(this.buffer.subarray(headerEnd + 4, headerEnd + 4 + length).toString()));
            this.buffer = this.buffer.subarray(headerEnd + 4 + length);
            this.waiters.forEach((wake) => wake());
        }
    }
    // The first unread message that matches; it is then read.
    waitFor(match, what, timeout = 10000) {
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => { done(); reject(new Error(`timeout waiting for ${what}; unread: ${JSON.stringify(this.messages)}`)); }, timeout);
            const check = () => {
                const index = this.messages.findIndex(match);
                if (index < 0) return;
                const [message] = this.messages.splice(index, 1);
                done();
                resolve(message);
            };
            const done = () => { clearTimeout(timer); this.waiters = this.waiters.filter((wake) => wake !== check); };
            this.waiters.push(check);
            check();
        });
    }
    event(name) { return this.waitFor((message) => message.type === "event" && message.event === name, `event ${name}`); }
    async response(command, args = {}) {
        const seq = this.sequence++;
        const body = JSON.stringify({ seq, type: "request", command, arguments: args });
        this.socket.write(`Content-Length: ${Buffer.byteLength(body)}\r\n\r\n${body}`);
        return this.waitFor((message) => message.type === "response" && message.request_seq === seq, `response ${command}`);
    }
    async request(command, args = {}) {
        const response = await this.response(command, args);
        assert.ok(response.success, `${command} failed: ${response.message}`);
        return response.body;
    }
    async stopped(reason) {
        const stop = await this.event("stopped");
        assert.equal(stop.body.reason, reason, `stopped for ${stop.body.reason}, not ${reason}`);
        return stop.body;
    }
    async locals(frame) {
        const { scopes } = await this.request("scopes", { frameId: frame });
        const { variables } = await this.request("variables", { variablesReference: scopes[0].variablesReference });
        return Object.fromEntries(variables.map((variable) => [variable.name, variable.value]));
    }
    async frames() { return (await this.request("stackTrace", { threadId: 1 })).stackFrames; }
}

const source = `class Game {
    static int helper(int n) {
        int doubled = n * 2;
        return doubled;
    }
    static int run() {
        int total = 0;
        for (int i = 0; i < 2; i += 1) {
            total += helper(i);
        }
        return total;
    }
}
int result = Game.run();
printf("result=%d\\n", result);
int[] items = {1};
int bad = items[5];
`;

const directory = fs.mkdtempSync(path.join(os.tmpdir(), "luasharp-dap-"));
const script = path.join(directory, "debug.ls");
fs.writeFileSync(script, source);
const child = spawn(executable, ["run", script, "--debug", "--port", "0", "-o", path.join(directory, "debug.c")], { stdio: ["ignore", "pipe", "pipe"] });
let stdout = "", stderr = "";
child.stderr.on("data", (chunk) => { stderr += chunk; });
const exited = new Promise((resolve) => child.on("exit", resolve));
try {
    const port = await new Promise((resolve, reject) => {
        child.stdout.on("data", (chunk) => {
            stdout += chunk;
            const found = /Waiting for a debugger on port (\d+)/.exec(stdout);
            if (found) resolve(Number(found[1]));
        });
        child.on("exit", () => reject(new Error(`LuaSharp exited before it listened:\n${stdout}\n${stderr}`)));
    });
    const socket = net.connect(port, "127.0.0.1");
    await new Promise((resolve) => socket.on("connect", resolve));
    const client = new DapClient(socket);

    const capabilities = await client.request("initialize", { adapterID: "luasharp", linesStartAt1: true });
    assert.equal(capabilities.supportsConfigurationDoneRequest, true);
    assert.equal(capabilities.supportsEvaluateForHovers, true);
    await client.event("initialized");
    await client.request("attach", {});
    const { breakpoints } = await client.request("setBreakpoints", { source: { path: script.toUpperCase() }, breakpoints: [{ line: 2 }, { line: 40 }] });
    assert.deepEqual(breakpoints.map((breakpoint) => [breakpoint.verified, breakpoint.line]), [[true, 3], [false, 40]], "breakpoints bound to the wrong lines");
    await client.request("setExceptionBreakpoints", { filters: ["errors"] });
    await client.request("configurationDone");

    await client.stopped("breakpoint");
    assert.deepEqual((await client.request("threads")).threads, [{ id: 1, name: "script" }]);
    let frames = await client.frames();
    assert.equal(frames.length, 3, `expected helper, run and the top level: ${JSON.stringify(frames)}`);
    assert.match(frames[0].name, /helper/);
    assert.equal(frames[0].line, 3);
    assert.equal(path.resolve(frames[0].source.path).toLowerCase(), script.toLowerCase());
    assert.match(frames[1].name, /run/);
    assert.equal(frames[1].line, 9);
    assert.deepEqual(await client.locals(0), { n: "0" }, "doubled is in scope after its declaration");
    assert.equal((await client.locals(1)).total, "0");

    await client.request("next", { threadId: 1 });
    await client.stopped("step");
    frames = await client.frames();
    assert.equal(frames[0].line, 4);
    assert.deepEqual(await client.locals(0), { n: "0", doubled: "0" });

    await client.request("stepOut", { threadId: 1 });
    await client.stopped("step");
    frames = await client.frames();
    assert.match(frames[0].name, /run/, `stepOut stopped in ${frames[0].name}`);

    // The second call of helper.
    await client.request("continue", { threadId: 1 });
    await client.stopped("breakpoint");
    assert.equal((await client.locals(0)).n, "1");
    assert.equal((await client.request("evaluate", { expression: "n", frameId: 0, context: "hover" })).result, "1");
    assert.equal((await client.request("evaluate", { expression: "total", frameId: 1, context: "watch" })).result, "0");
    const missing = await client.response("evaluate", { expression: "nope", frameId: 0, context: "watch" });
    assert.ok(!missing.success && /nope/.test(missing.message), `an unknown name evaluated: ${JSON.stringify(missing)}`);

    await client.request("setBreakpoints", { source: { path: script }, breakpoints: [] });
    await client.request("continue", { threadId: 1 });
    const output = await client.event("output");
    assert.equal(output.body.output, "result=2\n");
    const error = await client.stopped("exception");
    assert.match(error.text, /index out of range/);
    frames = await client.frames();
    assert.equal(frames[0].line, 17);

    await client.request("continue", { threadId: 1 });
    await client.waitFor((message) => message.event === "output" && message.body.category === "stderr", "stderr output");
    await client.event("terminated");
    await client.request("disconnect", {});
    socket.end();
    assert.equal(await exited, 1, `a runtime error must fail the run:\n${stdout}\n${stderr}`);
    assert.match(stdout, /result=2/);
    assert.match(stderr, /index out of range/);
    console.log("PASS LuaSharp DAP: attach, breakpoints, stack, locals, step, output, exceptions, and terminated");
} finally {
    child.kill();
    fs.rmSync(directory, { recursive: true, force: true });
}
