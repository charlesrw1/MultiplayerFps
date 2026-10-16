#include "Compiler.h"
#include "Builtins.h"
#include "Semantic.h"

#include <windows.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

namespace {
std::unordered_map<std::string, std::string> documents;

luasharp::Analysis AnalyzeForEditor(const std::string& text) { return luasharp::Analyze(text); }

bool ReadExact(char* buffer, DWORD count) {
    while (count != 0) { DWORD read{}; if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, count, &read, nullptr) || read == 0) return false; buffer += read; count -= read; }
    return true;
}
std::optional<json> ReadMessage() {
    std::string header; char character{};
    while (header.size() < 8192 && ReadExact(&character, 1)) { header += character; if (header.ends_with("\r\n\r\n")) break; }
    const auto match = header.find("Content-Length:"); if (match == std::string::npos) return std::nullopt;
    const auto lineEnd = header.find("\r\n", match); const auto length = std::stoul(header.substr(match + 15, lineEnd - match - 15));
    std::string body(length, '\0'); if (!ReadExact(body.data(), static_cast<DWORD>(body.size()))) return std::nullopt;
    return json::parse(body, nullptr, false);
}
void Send(const json& message) {
    const auto body = message.dump(), frame = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    DWORD written{}; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr);
}
size_t OffsetAt(const std::string& text, const json& position) {
    size_t offset{}; for (size_t line{}; line < position.value("line", 0U) && offset < text.size(); ++line) { const auto newline = text.find('\n', offset); offset = newline == std::string::npos ? text.size() : newline + 1; }
    return (std::min)(offset + position.value("character", 0U), text.size());
}
json PositionAt(const std::string& text, size_t offset) {
    offset = (std::min)(offset, text.size()); size_t line{}, lineStart{};
    while (lineStart < offset) { const auto newline = text.find('\n', lineStart); if (newline == std::string::npos || newline >= offset) break; ++line; lineStart = newline + 1; }
    return {{"line", line}, {"character", offset - lineStart}};
}
void UpdateDocument(const json& params, bool opened) {
    const auto& document = params["textDocument"]; const auto uri = document.value("uri", "");
    if (opened) { documents[uri] = document.value("text", ""); return; }
    auto& text = documents[uri];
    for (const auto& change : params.value("contentChanges", json::array())) {
        if (!change.contains("range")) { text = change.value("text", ""); continue; }
        const auto start = OffsetAt(text, change["range"]["start"]), end = OffsetAt(text, change["range"]["end"]);
        text.replace(start, end - start, change.value("text", ""));
    }
}
std::string LineAt(const std::string& text, size_t wanted) {
    size_t offset{}; for (size_t line{}; line < wanted; ++line) { offset = text.find('\n', offset); if (offset == std::string::npos) return {}; ++offset; }
    const auto end = text.find('\n', offset); return text.substr(offset, end == std::string::npos ? std::string::npos : end - offset);
}
std::string WordAt(const std::string& text, const json& position) {
    const auto line = LineAt(text, position.value("line", 0U)); size_t at = (std::min)(static_cast<size_t>(position.value("character", 0U)), line.size());
    while (at > 0 && (std::isalnum(static_cast<unsigned char>(line[at - 1])) || line[at - 1] == '_')) --at;
    size_t end = static_cast<size_t>(position.value("character", 0U)); end = (std::min)(end, line.size());
    while (end < line.size() && (std::isalnum(static_cast<unsigned char>(line[end])) || line[end] == '_')) ++end;
    return line.substr(at, end - at);
}
int TokenKind(luasharp::SymbolKind kind) {
    switch (kind) { case luasharp::SymbolKind::Class: return 0; case luasharp::SymbolKind::Type: return 1; case luasharp::SymbolKind::Method: return 3; case luasharp::SymbolKind::Field: return 4; case luasharp::SymbolKind::Parameter: return 6; default: return 5; }
}
json Location(const std::string& uri, const luasharp::Symbol& symbol) {
    return {{"uri", uri}, {"range", {{"start", {{"line", symbol.line}, {"character", symbol.character}}}, {"end", {{"line", symbol.line}, {"character", symbol.character + symbol.name.size()}}}}}};
}
void PublishDiagnostics(const std::string& uri) {
    json diagnostics = json::array();
    const auto analysis = luasharp::Analyze(documents[uri]);
    for (const auto& diagnostic : analysis.diagnostics) {
        const auto start = PositionAt(documents[uri], diagnostic.span.offset);
        const auto end = PositionAt(documents[uri], diagnostic.span.End() > diagnostic.span.offset ? diagnostic.span.End() : diagnostic.span.offset + 1);
        diagnostics.push_back({{"severity", diagnostic.severity == luasharp::DiagnosticSeverity::Error ? 1 : 2}, {"message", diagnostic.message}, {"source", "LuaSharp"}, {"range", {{"start", start}, {"end", end}}}});
    }
    Send({{"jsonrpc", "2.0"}, {"method", "textDocument/publishDiagnostics"}, {"params", {{"uri", uri}, {"diagnostics", diagnostics}}}});
}
json Completions(const std::string& text, const json& position) {
    const auto analysis = AnalyzeForEditor(text); const auto offset = OffsetAt(text, position); const auto line = LineAt(text, position.value("line", 0U)); const auto character = (std::min)(static_cast<size_t>(position.value("character", 0U)), line.size());
    json result = json::array();
    if (character > 0 && line[character - 1] == '.') {
        std::optional<std::string> type;
        if (offset >= 2) if (const auto* expression = luasharp::FindExpression(analysis, offset - 2); expression && !expression->type.empty()) type = expression->type;
        if (!type) {
            size_t end = character - 1, begin = end; while (begin > 0 && (std::isalnum(static_cast<unsigned char>(line[begin - 1])) || line[begin - 1] == '_')) --begin;
            type = luasharp::ResolveVariableType(analysis, line.substr(begin, end - begin), offset);
        }
        if (type) {
            const auto members = analysis.members.find(*type);
            if (members != analysis.members.end()) for (const auto& member : members->second) {
                const auto signature = member.kind == luasharp::SymbolKind::Method ? "(" + [&] { std::string value; for (size_t i{}; i < member.parameters.size(); ++i) { if (i) value += ", "; value += member.parameters[i]; } return value; }() + ")" : "";
                json item{{"label", member.name + signature}, {"kind", member.kind == luasharp::SymbolKind::Method ? 2 : 5}, {"detail", member.type + signature}};
                if (member.kind == luasharp::SymbolKind::Method) { item["insertTextFormat"] = 2; item["insertText"] = member.parameters.empty() ? member.name + "()" : member.name + "(${1})"; }
                result.push_back(std::move(item));
            }
        }
        return result;
    }
    const char* words[] = {"class", "public", "private", "auto", "new", "if", "else", "return", "void", "null"};
    for (const auto* word : words) result.push_back({{"label", word}, {"kind", 14}});
    for (const auto& type : luasharp::BuiltinTypes()) result.push_back({{"label", type}, {"kind", 7}, {"detail", "built-in type"}});
    for (const auto& symbol : analysis.symbols) result.push_back({{"label", symbol.name}, {"kind", symbol.kind == luasharp::SymbolKind::Class ? 7 : symbol.kind == luasharp::SymbolKind::Method ? 2 : 6}, {"detail", symbol.type}});
    return result;
}
json SignatureHelp(const std::string& text, const json& position) {
    const auto analysis = AnalyzeForEditor(text); const auto cursor = OffsetAt(text, position); const auto& tokens = analysis.syntax.tokens;
    size_t limit{}; while (limit < tokens.size() && tokens[limit].span.offset < cursor) ++limit;
    size_t open = tokens.size(), depth{};
    for (size_t i = limit; i-- > 0;) { if (tokens[i].text == ")") ++depth; else if (tokens[i].text == "(") { if (depth == 0) { open = i; break; } --depth; } }
    if (open < 3 || tokens[open - 1].kind != luasharp::SyntaxTokenKind::Identifier || tokens[open - 2].text != ".") return nullptr;
    const auto* receiver = luasharp::FindExpression(analysis, tokens[open - 3].span.offset); if (!receiver || receiver->type.empty()) return nullptr;
    const auto members = analysis.members.find(receiver->type); if (members == analysis.members.end()) return nullptr;
    const auto member = std::find_if(members->second.begin(), members->second.end(), [&](const auto& symbol) { return symbol.name == tokens[open - 1].text && symbol.kind == luasharp::SymbolKind::Method; });
    if (member == members->second.end()) return nullptr;
    std::string label = member->name + "("; for (size_t i{}; i < member->parameters.size(); ++i) { if (i) label += ", "; label += member->parameters[i]; } label += ")";
    json parameters = json::array(); for (const auto& parameter : member->parameters) parameters.push_back({{"label", parameter}});
    size_t activeParameter{}, argumentDepth{};
    for (size_t i = open + 1; i < limit; ++i) { if (tokens[i].text == "(" || tokens[i].text == "[" || tokens[i].text == "{") ++argumentDepth; else if (tokens[i].text == ")" || tokens[i].text == "]" || tokens[i].text == "}") { if (argumentDepth) --argumentDepth; } else if (tokens[i].text == "," && argumentDepth == 0) ++activeParameter; }
    return {{"signatures", json::array({{{"label", label}, {"parameters", parameters}}})}, {"activeSignature", 0}, {"activeParameter", activeParameter}};
}
json SemanticTokens(const std::string& text) {
    const auto analysis = AnalyzeForEditor(text);
    json data = json::array(); size_t previousLine{}, previousCharacter{}; bool emitted = false;
    for (const auto& token : analysis.syntax.tokens) {
        if (token.kind != luasharp::SyntaxTokenKind::Identifier) continue;
        const luasharp::Symbol* symbol = nullptr;
        const auto declaration = std::find_if(analysis.symbols.begin(), analysis.symbols.end(), [&](const auto& candidate) { return candidate.span.offset == token.span.offset; });
        if (declaration != analysis.symbols.end()) symbol = &*declaration;
        else if (const auto* expression = luasharp::FindExpression(analysis, token.span.offset); expression && expression->symbolIndex && *expression->symbolIndex < analysis.symbols.size()) symbol = &analysis.symbols[*expression->symbolIndex];
        if (!symbol) continue;
        const auto location = PositionAt(text, token.span.offset); const size_t line = location["line"], character = location["character"];
        data.push_back(emitted ? line - previousLine : line); data.push_back(emitted && line == previousLine ? character - previousCharacter : character); data.push_back(token.span.length); data.push_back(TokenKind(symbol->kind)); data.push_back(0);
        previousLine = line; previousCharacter = character; emitted = true;
    }
    return {{"data", data}};
}
}

int main() {
    while (auto message = ReadMessage()) {
        const auto method = message->value("method", "");
        if (method == "initialize") Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", {{"capabilities", {{"textDocumentSync", 2}, {"completionProvider", {{"triggerCharacters", json::array({"."})}}}, {"signatureHelpProvider", {{"triggerCharacters", json::array({"(", ","})}}}, {"hoverProvider", true}, {"definitionProvider", true}, {"semanticTokensProvider", {{"legend", {{"tokenTypes", json::array({"class", "type", "function", "method", "property", "variable", "parameter"})}, {"tokenModifiers", json::array()}}}, {"full", true}}}}}}}});
        else if (method == "textDocument/didOpen") { UpdateDocument((*message)["params"], true); PublishDiagnostics((*message)["params"]["textDocument"].value("uri", "")); }
        else if (method == "textDocument/didChange") { UpdateDocument((*message)["params"], false); PublishDiagnostics((*message)["params"]["textDocument"].value("uri", "")); }
        else if (method == "textDocument/completion") { const auto& p = (*message)["params"]; const auto uri = p["textDocument"].value("uri", ""); Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", Completions(documents[uri], p["position"])}}); }
        else if (method == "textDocument/signatureHelp") { const auto& p = (*message)["params"]; const auto uri = p["textDocument"].value("uri", ""); Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", SignatureHelp(documents[uri], p["position"])}}); }
        else if (method == "textDocument/semanticTokens/full") { const auto& p = (*message)["params"]; Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", SemanticTokens(documents[p["textDocument"].value("uri", "")])}}); }
        else if (method == "textDocument/hover" || method == "textDocument/definition") {
            const auto& p = (*message)["params"]; const auto uri = p["textDocument"].value("uri", ""); const auto analysis = AnalyzeForEditor(documents[uri]); const auto offset = OffsetAt(documents[uri], p["position"]);
            const luasharp::Symbol* symbol = nullptr;
            if (const auto* expression = luasharp::FindExpression(analysis, offset); expression && expression->symbolIndex && *expression->symbolIndex < analysis.symbols.size()) symbol = &analysis.symbols[*expression->symbolIndex];
            if (!symbol) { const auto word = WordAt(documents[uri], p["position"]); const auto found = std::find_if(analysis.symbols.begin(), analysis.symbols.end(), [&](const auto& candidate) { return candidate.name == word; }); if (found != analysis.symbols.end()) symbol = &*found; }
            if (method == "textDocument/definition") Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", symbol ? json::array({Location(uri, *symbol)}) : json::array()}});
            else Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", symbol ? json{{"contents", {{"kind", "markdown"}, {"value", "`" + symbol->name + "` : `" + symbol->type + "`"}}}} : json(nullptr)}});
        } else if (method == "shutdown") Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", nullptr}});
    }
}
