import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, "..", "..");
const server = process.env.LUASHARP_LSP ?? path.join(root, "x64", "Debug", "LuaSharpLspNative.exe");
const fixture = process.env.LUASHARP_TEST_FILE ?? path.join(root, "LuaSharp", "example.ls");
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
function decodeSemanticTokens(data) {
    const result = []; let line = 0; let character = 0;
    for (let index = 0; index < data.length; index += 5) {
        line += data[index]; character = data[index] === 0 ? character + data[index + 1] : data[index + 1];
        result.push({ line, character, length: data[index + 2], type: data[index + 3] });
    }
    return result;
}
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
    const legend = initialized.result.capabilities.semanticTokensProvider.legend.tokenTypes;
    const classToken = legend.indexOf("class"), structToken = legend.indexOf("struct");
    assert.ok(classToken >= 0 && structToken >= 0, "semantic token legend needs class and struct");
    send({ jsonrpc: "2.0", method: "initialized", params: {} });
    const uri = pathToFileURL(fixture).toString();
    send({ jsonrpc: "2.0", method: "textDocument/didOpen", params: { textDocument: { uri, languageId: "luasharp", version: 1, text: source } } });
    const cleanDiagnostics = await waitFor(m => m.method === "textDocument/publishDiagnostics" && m.params.uri === uri, "initial diagnostics");
    assert.ok(cleanDiagnostics.params.diagnostics.every(d => d.severity !== 1), "real LuaSharp fixture must have no errors");
    assert.ok(cleanDiagnostics.params.diagnostics.some(d => /Unknown member 'blah' on Entity/.test(d.message) && d.severity === 2), "undeclared Entity member must be warned");
    const completionLine = source.split("\n").length;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 2 }, contentChanges: [{ text: `${source}\nm.` }] } });
    send({ jsonrpc: "2.0", id: 2, method: "textDocument/completion", params: { textDocument: { uri }, position: { line: completionLine, character: 2 } } });
    const completion = await waitFor(m => m.id === 2, "member completion");
    const labels = completion.result.map(item => item.label);
    assert.ok(labels.some(label => label.startsWith("myfunc(")) && labels.includes("integer"), `expected MyClass members, got: ${labels.join(", ")}`);
    send({ jsonrpc: "2.0", id: 3, method: "textDocument/semanticTokens/full", params: { textDocument: { uri } } });
    const tokens = await waitFor(m => m.id === 3, "semantic tokens");
    assert.ok(tokens.result.data.length >= 5, "semantic tokens were empty");
    const decodedTokens = decodeSemanticTokens(tokens.result.data);
    const sourceLines = source.split("\n");
    for (const [lineText, occurrence] of [["Entity e = null;", "field"], ["Entity[] array = null;", "list field"], ["int myfunc(Entity e) {", "parameter"], ["auto m = new MyClass();", "constructor"]]) {
        const line = sourceLines.indexOf(lineText); const name = lineText.includes("MyClass") ? "MyClass" : "Entity"; const character = lineText.indexOf(name);
        assert.ok(decodedTokens.some(token => token.line === line && token.character === character && token.length === name.length && token.type === classToken), `${occurrence} class type reference was not emitted as a semantic class token`);
    }
    for (const lineText of ["int integer;", "float number;", "table mytable = {};"]) {
        const line = sourceLines.indexOf(lineText); const name = lineText.split(" ")[0];
        assert.equal(decodedTokens.some(token => token.line === line && token.character === 0 && token.length === name.length), false, `${name} must retain grammar-provided built-in coloring`);
    }
    const referenceLine = source.split("\n").length;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 3 }, contentChanges: [{ text: `${source}\nMyClass` }] } });
    send({ jsonrpc: "2.0", id: 4, method: "textDocument/definition", params: { textDocument: { uri }, position: { line: referenceLine, character: 2 } } });
    const definition = await waitFor(m => m.id === 4, "definition");
    const classLine = source.split("\n").findIndex(line => line.startsWith("class MyClass"));
    assert.equal(definition.result[0].range.start.line, classLine, "class definition location was incorrect");
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
    const stringCompletionSource = `${source}\nstring text = "";\ntext.`;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 8 }, contentChanges: [{ text: stringCompletionSource }] } });
    send({ jsonrpc: "2.0", id: 6, method: "textDocument/completion", params: { textDocument: { uri }, position: { line: stringCompletionSource.split("\n").length - 1, character: 5 } } });
    const stringCompletion = await waitFor(m => m.id === 6, "string member completion");
    assert.ok(stringCompletion.result.some(item => item.label.startsWith("contains(")), "string methods missing from completion");
    const listCompletionSource = `${source}\nEntity[] values = {};\nvalues.`;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 9 }, contentChanges: [{ text: listCompletionSource }] } });
    send({ jsonrpc: "2.0", id: 7, method: "textDocument/completion", params: { textDocument: { uri }, position: { line: listCompletionSource.split("\n").length - 1, character: 7 } } });
    const listCompletion = await waitFor(m => m.id === 7, "list member completion");
    assert.ok(listCompletion.result.some(item => item.label.startsWith("push_back(")) && listCompletion.result.some(item => item.label === "size()"), "list methods missing from completion");
    const nativeCompletionSource = `${source}\nVec3.`;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 10 }, contentChanges: [{ text: nativeCompletionSource }] } });
    send({ jsonrpc: "2.0", id: 8, method: "textDocument/completion", params: { textDocument: { uri }, position: { line: nativeCompletionSource.split("\n").length - 1, character: 5 } } });
    const nativeCompletion = await waitFor(m => m.id === 8, "native static completion");
    assert.ok(nativeCompletion.result.some(item => item.label.startsWith("new(")) && nativeCompletion.result.some(item => item.label.startsWith("splat(")), "native static methods missing from completion");
    const listHoverSource = `${source}\nint[] values = {};\nvalues.push_back(1);`;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 11 }, contentChanges: [{ text: listHoverSource }] } });
    send({ jsonrpc: "2.0", id: 9, method: "textDocument/hover", params: { textDocument: { uri }, position: { line: listHoverSource.split("\n").length - 1, character: 10 } } });
    const listHover = await waitFor(m => m.id === 9, "list method hover");
    assert.match(listHover.result.contents.value, /void push_back\(int value\)/, "list method hover must include its return type and parameters");
    const printfSource = `${source}\nprintf("hi\\n");`;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 12 }, contentChanges: [{ text: printfSource }] } });
    send({ jsonrpc: "2.0", id: 91, method: "textDocument/hover", params: { textDocument: { uri }, position: { line: printfSource.split("\n").length - 1, character: 2 } } });
    const printfHover = await waitFor(m => m.id === 91, "builtin function hover");
    assert.match(printfHover.result.contents.value, /void printf\(string format/, "builtin function hover must show its signature");
    const partialSource = `${source}\nint[] partialValues = {};\npartialValues.pu`;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 13 }, contentChanges: [{ text: partialSource }] } });
    send({ jsonrpc: "2.0", id: 10, method: "textDocument/completion", params: { textDocument: { uri }, position: { line: partialSource.split("\n").length - 1, character: "partialValues.pu".length } } });
    const partialCompletion = await waitFor(m => m.id === 10, "partial member completion");
    assert.ok(partialCompletion.result.some(item => item.label.startsWith("push_back(")), "member completion failed after a typed prefix");

    const constructorSource = `${source}\nnew MyClass(`;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 13 }, contentChanges: [{ text: constructorSource }] } });
    send({ jsonrpc: "2.0", id: 11, method: "textDocument/signatureHelp", params: { textDocument: { uri }, position: { line: constructorSource.split("\n").length - 1, character: "new MyClass(".length } } });
    const constructorSignature = await waitFor(m => m.id === 11, "constructor signature");
    assert.match(constructorSignature.result.signatures[0].label, /MyClass\(\) -> MyClass/, "constructor signature help was missing");

    const implicitSource = "class Signatures { void target(int count) {} void caller() { target( } }";
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 14 }, contentChanges: [{ text: implicitSource }] } });
    send({ jsonrpc: "2.0", id: 12, method: "textDocument/signatureHelp", params: { textDocument: { uri }, position: { line: 0, character: implicitSource.indexOf("target(", 30) + "target(".length } } });
    const implicitSignature = await waitFor(m => m.id === 12, "implicit method signature");
    assert.match(implicitSignature.result.signatures[0].label, /target\(int count\) -> void/, "implicit method signature help was missing");

    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 15 }, contentChanges: [{ text: source }] } });
    const entityFieldLine = source.split("\n").indexOf("Entity e = null;");
    send({ jsonrpc: "2.0", id: 13, method: "textDocument/references", params: { textDocument: { uri }, position: { line: entityFieldLine, character: 2 }, context: { includeDeclaration: true } } });
    const references = await waitFor(m => m.id === 13, "class references");
    assert.ok(references.result.length >= 4, "class type references were not returned");
    send({ jsonrpc: "2.0", id: 14, method: "textDocument/rename", params: { textDocument: { uri }, position: { line: entityFieldLine, character: 2 }, newName: "Actor" } });
    const rename = await waitFor(m => m.id === 14, "class rename");
    assert.equal(rename.result.changes[uri].length, references.result.length, "rename did not cover every class reference");
    send({ jsonrpc: "2.0", id: 15, method: "textDocument/documentSymbol", params: { textDocument: { uri } } });
    const documentSymbols = await waitFor(m => m.id === 15, "document symbols");
    assert.ok(documentSymbols.result.some(symbol => symbol.name === "MyClass") && documentSymbols.result.some(symbol => symbol.name === "myfunc"), "document symbols omitted class members");
    send({ jsonrpc: "2.0", id: 16, method: "workspace/symbol", params: { query: "MyClass" } });
    const workspaceSymbols = await waitFor(m => m.id === 16, "workspace symbols");
    assert.ok(workspaceSymbols.result.some(symbol => symbol.name === "MyClass"), "workspace symbol search omitted an open document class");

    const unicodeSource = 'string emoji = "😀"; int bad = "x";';
    messages.splice(0);
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri, version: 16 }, contentChanges: [{ text: unicodeSource }] } });
    const unicodeDiagnostics = await waitFor(m => m.method === "textDocument/publishDiagnostics" && m.params.diagnostics.some(d => /Cannot assign/.test(d.message)), "UTF-16 diagnostic");
    assert.equal(unicodeDiagnostics.params.diagnostics.find(d => /Cannot assign/.test(d.message)).range.start.character, unicodeSource.indexOf('"x"'), "diagnostic character was not encoded as UTF-16");

    const sharedUri = pathToFileURL(path.join(root, "LuaSharp", "shared-test.ls")).toString();
    const consumerUri = pathToFileURL(path.join(root, "LuaSharp", "consumer-test.ls")).toString();
    const sharedSource = "class Shared { int value; void set(int next) { value = next; } }";
    const consumerSource = "Shared item = new Shared(); int current = item.value; item.set(2);";
    messages.splice(0);
    send({ jsonrpc: "2.0", method: "textDocument/didOpen", params: { textDocument: { uri: sharedUri, languageId: "luasharp", version: 1, text: sharedSource } } });
    await waitFor(m => m.method === "textDocument/publishDiagnostics" && m.params.uri === sharedUri, "shared document diagnostics");
    send({ jsonrpc: "2.0", method: "textDocument/didOpen", params: { textDocument: { uri: consumerUri, languageId: "luasharp", version: 1, text: consumerSource } } });
    const consumerDiagnostics = await waitFor(m => m.method === "textDocument/publishDiagnostics" && m.params.uri === consumerUri, "cross-file diagnostics");
    assert.equal(consumerDiagnostics.params.diagnostics.some(d => /Unknown type|Unknown member/.test(d.message)), false, "cross-file types or members were not indexed");
    send({ jsonrpc: "2.0", id: 17, method: "textDocument/definition", params: { textDocument: { uri: consumerUri }, position: { line: 0, character: 2 } } });
    const crossDefinition = await waitFor(m => m.id === 17, "cross-file definition");
    assert.equal(crossDefinition.result[0].uri, sharedUri, "cross-file type definition resolved to the wrong document");
    send({ jsonrpc: "2.0", id: 18, method: "textDocument/references", params: { textDocument: { uri: consumerUri }, position: { line: 0, character: 2 }, context: { includeDeclaration: true } } });
    const crossReferences = await waitFor(m => m.id === 18, "cross-file references");
    assert.ok(crossReferences.result.some(location => location.uri === sharedUri) && crossReferences.result.some(location => location.uri === consumerUri), "cross-file references did not cover declaration and use sites");
    // The consumer's cached analysis must pick up member changes in the shared file.
    const consumerCompletionSource = `${consumerSource}\nitem.`;
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri: consumerUri, version: 2 }, contentChanges: [{ text: consumerCompletionSource }] } });
    send({ jsonrpc: "2.0", id: 19, method: "textDocument/completion", params: { textDocument: { uri: consumerUri }, position: { line: 1, character: 5 } } });
    const beforeChange = await waitFor(m => m.id === 19, "cross-file completion before change");
    assert.equal(beforeChange.result.some(item => item.label === "added"), false, "member appeared before it was declared");
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri: sharedUri, version: 2 }, contentChanges: [{ text: sharedSource.replace("int value;", "int value; int added;") }] } });
    send({ jsonrpc: "2.0", id: 20, method: "textDocument/completion", params: { textDocument: { uri: consumerUri }, position: { line: 1, character: 5 } } });
    const afterChange = await waitFor(m => m.id === 20, "cross-file completion after change");
    assert.ok(afterChange.result.some(item => item.label === "added"), "cached analysis missed a member added in another file");
    const indexedUri = pathToFileURL(path.join(root, "LuaSharp", "indexed-test.ls")).toString();
    const indexedConsumerUri = pathToFileURL(path.join(root, "LuaSharp", "indexed-consumer-test.ls")).toString();
    send({ jsonrpc: "2.0", method: "luasharp/indexWorkspace", params: { documents: [{ uri: indexedUri, text: "class IndexedType { string label; }" }] } });
    messages.splice(0);
    send({ jsonrpc: "2.0", method: "textDocument/didOpen", params: { textDocument: { uri: indexedConsumerUri, languageId: "luasharp", version: 1, text: "IndexedType value = new IndexedType(); string label = value.label;" } } });
    const indexedDiagnostics = await waitFor(m => m.method === "textDocument/publishDiagnostics" && m.params.uri === indexedConsumerUri, "indexed workspace diagnostics");
    assert.equal(indexedDiagnostics.params.diagnostics.some(d => /Unknown type|Unknown member/.test(d.message)), false, "unopened workspace files were not included in semantic indexing");
    const indexedConsumerDiagnostics = async description => { const found = await waitFor(m => m.method === "textDocument/publishDiagnostics" && m.params.uri === indexedConsumerUri, description); messages.splice(0); return found.params.diagnostics.some(d => /Unknown type 'IndexedType'/.test(d.message)); };
    messages.splice(0);
    send({ jsonrpc: "2.0", method: "luasharp/indexDocument", params: { uri: indexedUri, text: "class RenamedType { string label; }" } });
    assert.equal(await indexedConsumerDiagnostics("diagnostics after indexDocument"), true, "indexDocument did not update another file's exported types");
    send({ jsonrpc: "2.0", method: "luasharp/indexDocument", params: { uri: indexedUri, text: "class IndexedType { string label; }" } });
    assert.equal(await indexedConsumerDiagnostics("diagnostics after restoring indexDocument"), false, "indexDocument did not restore the exported type");
    send({ jsonrpc: "2.0", method: "luasharp/removeDocument", params: { uri: indexedUri } });
    assert.equal(await indexedConsumerDiagnostics("diagnostics after removeDocument"), true, "removeDocument kept the deleted file's types");
    const nativeUri = pathToFileURL(path.join(root, "LuaSharp", "native-types-test.ls")).toString();
    const nativeSource = "Vec3 v = Vec3(1, 2, 3);\nMat4x4 m = Mat4x4.identity();\nfloat a = math.sqrt(2.0);\nVec2i[] list = null;\nstruct Cell { int x; }\nCell c = Cell();";
    send({ jsonrpc: "2.0", method: "textDocument/didOpen", params: { textDocument: { uri: nativeUri, languageId: "luasharp", version: 1, text: nativeSource } } });
    send({ jsonrpc: "2.0", id: 21, method: "textDocument/semanticTokens/full", params: { textDocument: { uri: nativeUri } } });
    const nativeTokens = decodeSemanticTokens((await waitFor(m => m.id === 21, "native type semantic tokens")).result.data);
    const nativeLines = nativeSource.split("\n");
    for (const [line, name, occurrence, expected] of [[0, "Vec3", 0, structToken], [0, "Vec3", 1, structToken], [1, "Mat4x4", 0, structToken], [1, "Mat4x4", 1, structToken],
        [2, "math", 0, classToken], [3, "Vec2i", 0, structToken], [4, "Cell", 0, structToken], [5, "Cell", 0, structToken], [5, "Cell", 1, structToken]]) {
        let character = -1; for (let i = 0; i <= occurrence; ++i) character = nativeLines[line].indexOf(name, character + 1);
        assert.ok(nativeTokens.some(token => token.line === line && token.character === character && token.length === name.length && token.type === expected), `type '${name}' (line ${line}, use ${occurrence + 1}) was not emitted as ${legend[expected]}`);
    }
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri: nativeUri, version: 2 }, contentChanges: [{ text: `${nativeSource}\nmat` }] } });
    send({ jsonrpc: "2.0", id: 22, method: "textDocument/completion", params: { textDocument: { uri: nativeUri }, position: { line: nativeLines.length, character: 3 } } });
    const identifierCompletion = (await waitFor(m => m.id === 22, "identifier completion")).result;
    for (const name of ["math", "Mat4x4"]) assert.ok(identifierCompletion.some(item => item.label === name), `identifier completion is missing built-in '${name}'`);
    const staticSource = "class Counter { static int count = 0; int id; static int next() { return count + 1; } }\nCounter.";
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri: nativeUri, version: 3 }, contentChanges: [{ text: staticSource }] } });
    send({ jsonrpc: "2.0", id: 23, method: "textDocument/completion", params: { textDocument: { uri: nativeUri }, position: { line: 1, character: "Counter.".length } } });
    const staticLabels = (await waitFor(m => m.id === 23, "static member completion")).result.map(item => item.label);
    assert.ok(staticLabels.includes("count") && staticLabels.some(label => label.startsWith("next(")) && !staticLabels.includes("id"), `static completion should list static members only, got: ${staticLabels.join(", ")}`);
    // The grammar must not copy built-in names; the server colors them. Primitive types are the one allowed list.
    const grammarWords = new Set(readFileSync(path.join(root, "LuaSharp", "vscode-extension", "syntaxes", "luasharp.tmLanguage.json"), "utf8").match(/[A-Za-z_][A-Za-z0-9_]*/g));
    const primitives = new Set(["int", "float", "bool", "string", "table"]);
    const builtinNames = identifierCompletion.filter(item => /^built-in/.test(item.detail ?? "") || item.kind === 3).map(item => item.label.split("(")[0]).filter(name => !primitives.has(name));
    assert.ok(builtinNames.includes("Vec3") && builtinNames.includes("math"), "completion did not list the built-in names the grammar check needs");
    const hardcoded = builtinNames.filter(name => grammarWords.has(name));
    assert.deepEqual(hardcoded, [], `grammar hardcodes built-in names: ${hardcoded.join(", ")}`);
    // A file under luasharp.json sees the declarations it names (read from disk) and the files of its project only.
    const project = path.join(os.tmpdir(), "luasharp-lsp-project-test");
    rmSync(project, { recursive: true, force: true });
    mkdirSync(path.join(project, ".luasharp"), { recursive: true });
    writeFileSync(path.join(project, "luasharp.json"), JSON.stringify({ declarations: [".luasharp", "missing.ls"] }));
    const engineFile = path.join(project, ".luasharp", "engine.ls");
    writeFileSync(engineFile, "extern class Engine { static int Frame(); }");
    const gameUri = pathToFileURL(path.join(project, "game", "main.ls")).toString();
    const gameSource = "int frame = Engine.Frame();\nShared outside = null;";
    messages.splice(0);
    send({ jsonrpc: "2.0", method: "textDocument/didOpen", params: { textDocument: { uri: gameUri, languageId: "luasharp", version: 1, text: gameSource } } });
    const gameDiagnostics = (await waitFor(m => m.method === "textDocument/publishDiagnostics" && m.params.uri === gameUri, "project diagnostics")).params.diagnostics;
    assert.equal(gameDiagnostics.some(d => /Engine/.test(d.message)), false, `project declarations were not used: ${JSON.stringify(gameDiagnostics)}`);
    assert.ok(gameDiagnostics.some(d => /Unknown type 'Shared'/.test(d.message)), "a file outside the project leaked its types into the project");
    assert.match(stderr, /missing\.ls/, "a missing declaration path was not reported");
    send({ jsonrpc: "2.0", id: 24, method: "textDocument/definition", params: { textDocument: { uri: gameUri }, position: { line: 0, character: 14 } } });
    const engineDefinition = (await waitFor(m => m.id === 24, "declaration definition")).result;
    assert.equal(path.resolve(fileURLToPath(engineDefinition[0].uri)).toLowerCase(), path.resolve(engineFile).toLowerCase(), "definition did not open the declaration file");
    const sharedDiagnostics = async () => { messages.splice(0); send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri: sharedUri, version: 9 }, contentChanges: [{ text: "Engine e = null;" }] } }); return (await waitFor(m => m.method === "textDocument/publishDiagnostics" && m.params.uri === sharedUri, "outside diagnostics")).params.diagnostics; };
    assert.ok((await sharedDiagnostics()).some(d => /Unknown type 'Engine'/.test(d.message)), "project declarations leaked to a file outside the project");
    writeFileSync(engineFile, "extern class Engine { static int Frame(); static int Tick(); }");
    messages.splice(0);
    send({ jsonrpc: "2.0", method: "textDocument/didChange", params: { textDocument: { uri: gameUri, version: 2 }, contentChanges: [{ text: "int tick = Engine.Tick();" }] } });
    const reloaded = (await waitFor(m => m.method === "textDocument/publishDiagnostics" && m.params.uri === gameUri, "diagnostics after declaration change")).params.diagnostics;
    assert.deepEqual(reloaded, [], "a changed declaration file was not reloaded");
    rmSync(project, { recursive: true, force: true });
    console.log("PASS native LSP: luasharp.json projects, cross-file indexing, per-file index updates, built-in type tokens and completions, grammar has no built-in names, UTF-16 diagnostics, definitions, references, rename, symbols, tokens, signatures, hovers, and partial completions");
} finally { child.kill(); }
