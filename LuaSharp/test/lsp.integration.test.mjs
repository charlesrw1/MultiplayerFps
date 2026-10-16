import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, "..", "..");
const server = process.env.LUASHARP_LSP ?? path.join(root, "x64", "Debug", "LuaSharpLspNative.exe");
const fixture = process.env.LUASHARP_TEST_FILE ?? "D:\\Data\\scripts\\fps\\luasharpfile.ls";
assert.ok(existsSync(server), `Missing language server: ${server}`);
assert.ok(existsSync(fixture), `Missing fixture: ${fixture}`);
const source = readFileSync(fixture, "utf8");
const child = spawn(server, [], { stdio: ["pipe", "pipe", "pipe"] });
const messages = []; let buffer = Buffer.alloc(0); let stderr = "";
child.stderr.setEncoding("utf8"); child.stderr.on("data", chunk => { stderr += chunk; });
child.stdout.on("data", chunk => {
    buffer = Buffer.concat([buffer, chunk]);
    while (true) { const split = buffer.indexOf("\r\n\r\n"); if (split < 0) return; const length = Number(/Content-Length:\s*(\d+)/i.exec(buffer.subarray(0, split).toString("ascii"))?.[1]); if (!Number.isFinite(length) || buffer.length < split + 4 + length) return; messages.push(JSON.parse(buffer.subarray(split + 4, split + 4 + length).toString("utf8"))); buffer = buffer.subarray(split + 4 + length); }
});
function send(message) { const body = JSON.stringify(message); child.stdin.write(`Content-Length: ${Buffer.byteLength(body)}\r\n\r\n${body}`); }
async function waitFor(predicate, description) {
    const deadline = Date.now() + 10000;
    while (Date.now() < deadline) { const message = messages.find(predicate); if (message) return message; if (child.exitCode !== null) throw new Error(`Server exited ${child.exitCode}: ${stderr}`); await new Promise(resolve => setTimeout(resolve, 20)); }
    throw new Error(`Timed out waiting for ${description}: ${stderr}`);
}
try {
    send({ jsonrpc: "2.0", id: 1, method: "initialize", params: { processId: process.pid, rootUri: null, capabilities: {} } });
    const initialized = await waitFor(m => m.id === 1, "initialize");
    assert.equal(initialized.result.capabilities.definitionProvider, true);
    assert.ok(initialized.result.capabilities.semanticTokensProvider, "semantic tokens capability missing");
    send({ jsonrpc: "2.0", method: "initialized", params: {} });
    const uri = pathToFileURL(fixture).toString();
    send({ jsonrpc: "2.0", method: "textDocument/didOpen", params: { textDocument: { uri, languageId: "luasharp", version: 1, text: source } } });
    const cleanDiagnostics = await waitFor(m => m.method === "textDocument/publishDiagnostics" && m.params.uri === uri, "initial diagnostics");
    assert.deepEqual(cleanDiagnostics.params.diagnostics, [], "real LuaSharp fixture must type-check cleanly");
    const completionLine = source.split("\n").length;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 2 }, contentChanges: [{ text: `${source}\nm.` }] } });
    send({ jsonrpc: "2.0", id: 2, method: "textDocument/completion", params: { textDocument: { uri }, position: { line: completionLine, character: 2 } } });
    const completion = await waitFor(m => m.id === 2, "member completion");
    const labels = completion.result.map(item => item.label);
    assert.ok(labels.some(label => label.startsWith("myfunc(")) && labels.includes("integer"), `expected MyClass members, got: ${labels.join(", ")}`);
    send({ jsonrpc: "2.0", id: 3, method: "textDocument/semanticTokens/full", params: { textDocument: { uri } } });
    const tokens = await waitFor(m => m.id === 3, "semantic tokens");
    assert.ok(tokens.result.data.length >= 5, "semantic tokens were empty");
    const referenceLine = source.split("\n").length;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 3 }, contentChanges: [{ text: `${source}\nMyClass` }] } });
    send({ jsonrpc: "2.0", id: 4, method: "textDocument/definition", params: { textDocument: { uri }, position: { line: referenceLine, character: 2 } } });
    const definition = await waitFor(m => m.id === 4, "definition");
    assert.equal(definition.result[0].range.start.line, 0, "class definition location was incorrect");
    const signatureLine = source.split("\n").length;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 4 }, contentChanges: [{ text: `${source}\nm.myfunc(` }] } });
    send({ jsonrpc: "2.0", id: 5, method: "textDocument/signatureHelp", params: { textDocument: { uri }, position: { line: signatureLine, character: 9 } } });
    const signature = await waitFor(m => m.id === 5, "signature help");
    assert.match(signature.result.signatures[0].label, /myfunc\(Entity e\)/, "method parameter signature missing");
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 5 }, contentChanges: [{ text: `${source}\nint invalid = \"bad\";` }] } });
    const mismatch = await waitFor(m => m.method === "textDocument/publishDiagnostics" && m.params.diagnostics.some(d => /Cannot assign/.test(d.message)), "type mismatch diagnostic");
    assert.ok(mismatch.params.diagnostics.some(d => /Cannot assign/.test(d.message)));
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 6 }, contentChanges: [{ text: `${source}\nUnknownType value;\nm.missing();` }] } });
    const unknowns = await waitFor(m => m.method === "textDocument/publishDiagnostics" && m.params.diagnostics.some(d => /Unknown type/.test(d.message)) && m.params.diagnostics.some(d => /Unknown member/.test(d.message)), "unknown type and member diagnostics");
    assert.ok(unknowns.params.diagnostics.some(d => /Unknown type/.test(d.message) && d.severity === 1));
    assert.ok(unknowns.params.diagnostics.some(d => /Unknown member/.test(d.message) && d.severity === 2));
    const brokenLine = source.split("\n").length;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 7 }, contentChanges: [{ text: `${source}\nclass Broken` }] } });
    const syntax = await waitFor(m => m.method === "textDocument/publishDiagnostics" && m.params.diagnostics.some(d => /Expected '\{'/.test(d.message)), "syntax diagnostic");
    assert.equal(syntax.params.diagnostics.find(d => /Expected '\{'/.test(d.message)).range.start.line, brokenLine, "syntax error was not mapped to its source line");
    console.log("PASS native LSP: fixture diagnostics, members, definitions, semantic tokens, signatures, type/member/syntax errors");
} finally { child.kill(); }
