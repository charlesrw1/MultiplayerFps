import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const grammarPath = path.join(here, "..", "vscode-extension", "syntaxes", "luasharp.tmLanguage.json");
const grammar = JSON.parse(readFileSync(grammarPath, "utf8"));
const serialized = JSON.stringify(grammar);

for (const engineName of ["Entity", "Component", "get_owner"]) {
    assert.equal(serialized.includes(engineName), false, `${engineName} must not be hardcoded in the grammar`);
}

const primitivePattern = grammar.repository.types.patterns[0].match;
assert.equal(primitivePattern, "\\b(?:int|float|bool|string|table)\\b");
assert.ok(grammar.repository.comments.patterns.some(pattern => pattern.name === "comment.block.luasharp"), "block comments must be highlighted");
assert.ok(grammar.repository.declarations.patterns.some(pattern => pattern.captures?.["3"]?.name === "entity.name.type.class.luasharp"), "class declarations must be highlighted structurally");
assert.ok(grammar.repository.declarations.patterns.some(pattern => pattern.captures?.["3"]?.name === "entity.name.type.enum.luasharp"), "enum declarations must be highlighted structurally");
assert.ok(grammar.repository.members.patterns.some(pattern => pattern.name === "entity.name.function.method.luasharp" && pattern.match.includes("(?<=\\.)") && pattern.match.includes("(?=\\s*\\()")), "member calls such as push_back must be highlighted structurally");
assert.equal(grammar.repository.members.patterns.some(pattern => pattern.match.includes("push_back")), false, "built-in method names must not be hardcoded in the grammar");
assert.ok(grammar.repository.keywords.patterns.some(pattern => pattern.match.includes("while")), "while must be highlighted as a control keyword");
for (const keyword of ["for", "in", "break", "continue"]) {
    assert.ok(grammar.repository.keywords.patterns.some(pattern => pattern.name === "keyword.control.luasharp" && pattern.match.includes(keyword)), `${keyword} must be highlighted as a control keyword`);
}
for (const keyword of ["virtual", "override", "abstract", "sealed", "extern", "inout"]) {
    assert.ok(grammar.repository.keywords.patterns.some(pattern => pattern.name === "storage.modifier.luasharp" && pattern.match.includes(keyword)), `${keyword} must be highlighted as a modifier`);
}
assert.ok(grammar.repository.keywords.patterns.some(pattern => pattern.match.includes("is|as")), "is and as must be highlighted");
assert.ok(grammar.repository.operators.patterns.some(pattern => pattern.match.includes("\\.\\.")), "integer range operator must be highlighted");
assert.ok(grammar.repository.operators.patterns.some(pattern => pattern.match.includes("=>")), "lambda arrow must be highlighted");

console.log("PASS LuaSharp syntax grammar uses structural declarations and member highlighting without hardcoded methods");
