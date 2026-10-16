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

console.log("PASS LuaSharp syntax grammar has only language primitives and structural declarations");
