#include "Compiler.h"
#include "Builtins.h"
#include "Semantic.h"

#include <windows.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

using json = nlohmann::json;

namespace {
// Analyses are cached per document. A text change drops only that document's
// analyses; other documents re-analyze only when its exported types change.
struct Document {
    std::string text;
    std::unique_ptr<luasharp::Analysis> standalone; // without other files' types; source of exports
    std::vector<luasharp::ExternalType> exports;    // types this file offers the others
    std::unique_ptr<luasharp::Analysis> editor;     // with the other files' exports
    uint64_t editorRevision{};                      // exportsRevision the editor analysis was built against

    void SetText(std::string value) { text = std::move(value); standalone.reset(); editor.reset(); }
};
std::unordered_map<std::string, Document> documents;        // open in the editor
std::unordered_map<std::string, Document> indexedDocuments; // workspace files; open documents win
uint64_t exportsRevision = 1;                               // advances when the set of exported types changes

template <typename Visit> void ForEachDocument(Visit&& visit) {
    for (auto& [uri, document] : documents) visit(uri, document);
    for (auto& [uri, document] : indexedDocuments) if (!documents.contains(uri)) visit(uri, document);
}

std::vector<luasharp::ExternalType> ExportsOf(const luasharp::Analysis& analysis) {
    std::vector<luasharp::ExternalType> result;
    for (const auto& symbol : analysis.symbols) {
        if ((symbol.kind != luasharp::SymbolKind::Class && symbol.kind != luasharp::SymbolKind::Enum) || !symbol.span.length) continue;
        luasharp::ExternalType type{symbol.name}; type.isEnum = symbol.kind == luasharp::SymbolKind::Enum; type.isStruct = symbol.isStruct; if (const auto base = analysis.baseTypes.find(symbol.name); base != analysis.baseTypes.end()) type.baseType = base->second;
        if (const auto members = analysis.members.find(type.isEnum ? "$type:" + symbol.name : symbol.name); members != analysis.members.end()) for (const auto index : members->second) type.members.push_back(analysis.symbols[index]);
        if (!type.isEnum) {
            if (const auto statics = analysis.members.find("$type:" + symbol.name); statics != analysis.members.end()) for (const auto index : statics->second) type.members.push_back(analysis.symbols[index]);
            for (const auto& candidate : analysis.symbols) if (candidate.kind == luasharp::SymbolKind::Method && candidate.name == symbol.name && candidate.owner == symbol.name && candidate.span.length) type.members.push_back(candidate); // constructors
        }
        result.push_back(std::move(type));
    }
    return result;
}
// Positions are ignored: moving a declaration does not change what other files see.
bool SameExports(const std::vector<luasharp::ExternalType>& left, const std::vector<luasharp::ExternalType>& right) {
    const auto sameMember = [](const luasharp::Symbol& a, const luasharp::Symbol& b) { return a.name == b.name && a.type == b.type && a.kind == b.kind && a.parameters == b.parameters && a.owner == b.owner && a.access == b.access; };
    const auto sameType = [&](const luasharp::ExternalType& a, const luasharp::ExternalType& b) { return a.name == b.name && a.baseType == b.baseType && a.isEnum == b.isEnum && std::equal(a.members.begin(), a.members.end(), b.members.begin(), b.members.end(), sameMember); };
    return std::equal(left.begin(), left.end(), right.begin(), right.end(), sameType);
}
const luasharp::Analysis& Standalone(Document& document) {
    if (document.standalone) return *document.standalone;
    document.standalone = std::make_unique<luasharp::Analysis>(luasharp::Analyze(document.text));
    auto exports = ExportsOf(*document.standalone);
    if (!SameExports(exports, document.exports)) { document.exports = std::move(exports); ++exportsRevision; }
    return *document.standalone;
}
// Refreshes every document's exports first, so exportsRevision is stable for the
// rest of the request and earlier references from Editor() stay valid.
const luasharp::Analysis& Editor(const std::string& uri, Document& document) {
    ForEachDocument([](const std::string&, Document& value) { Standalone(value); });
    if (document.editor && document.editorRevision == exportsRevision) return *document.editor;
    std::vector<luasharp::ExternalType> external;
    ForEachDocument([&](const std::string& other, Document& value) { if (other != uri) external.insert(external.end(), value.exports.begin(), value.exports.end()); });
    document.editor = std::make_unique<luasharp::Analysis>(luasharp::Analyze(document.text, external));
    document.editorRevision = exportsRevision;
    return *document.editor;
}
const luasharp::Analysis& Editor(const std::string& uri) { return Editor(uri, documents[uri]); }

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
    size_t offset{}; while (offset < frame.size()) { DWORD written{}; if (!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), frame.data() + offset, static_cast<DWORD>(frame.size() - offset), &written, nullptr) || written == 0) break; offset += written; }
}
size_t OffsetAt(const std::string& text, const json& position) {
    size_t offset{}; for (size_t line{}; line < position.value("line", 0U) && offset < text.size(); ++line) { const auto newline = text.find('\n', offset); offset = newline == std::string::npos ? text.size() : newline + 1; }
    const size_t wanted = position.value("character", 0U); size_t units{};
    while (offset < text.size() && text[offset] != '\n' && units < wanted) {
        const auto lead = static_cast<unsigned char>(text[offset]); const size_t bytes = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
        const size_t nextUnits = units + (bytes == 4 ? 2 : 1); if (nextUnits > wanted) break; units = nextUnits; offset = (std::min)(offset + bytes, text.size());
    }
    return offset;
}
json PositionAt(const std::string& text, size_t offset) {
    offset = (std::min)(offset, text.size()); size_t line{}, lineStart{};
    while (lineStart < offset) { const auto newline = text.find('\n', lineStart); if (newline == std::string::npos || newline >= offset) break; ++line; lineStart = newline + 1; }
    size_t character{}, at = lineStart; while (at < offset) { const auto lead = static_cast<unsigned char>(text[at]); const size_t bytes = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4; character += bytes == 4 ? 2 : 1; at = (std::min)(at + bytes, offset); }
    return {{"line", line}, {"character", character}};
}
void UpdateDocument(const json& params, bool opened) {
    const auto& document = params["textDocument"]; const auto uri = document.value("uri", "");
    // Opening shadows any indexed copy, so the exported types may change.
    if (opened) { documents[uri].SetText(document.value("text", "")); ++exportsRevision; return; }
    auto text = std::move(documents[uri].text);
    for (const auto& change : params.value("contentChanges", json::array())) {
        if (!change.contains("range")) { text = change.value("text", ""); continue; }
        const auto start = OffsetAt(text, change["range"]["start"]), end = OffsetAt(text, change["range"]["end"]);
        text.replace(start, end - start, change.value("text", ""));
    }
    documents[uri].SetText(std::move(text));
}
std::string LineAt(const std::string& text, size_t wanted) {
    size_t offset{}; for (size_t line{}; line < wanted; ++line) { offset = text.find('\n', offset); if (offset == std::string::npos) return {}; ++offset; }
    const auto end = text.find('\n', offset); return text.substr(offset, end == std::string::npos ? std::string::npos : end - offset);
}
std::string WordAt(const std::string& text, const json& position) {
    size_t at = OffsetAt(text, position), begin = at, end = at;
    while (begin > 0 && (std::isalnum(static_cast<unsigned char>(text[begin - 1])) || text[begin - 1] == '_')) --begin;
    while (end < text.size() && (std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_')) ++end;
    return text.substr(begin, end - begin);
}
int TokenKind(luasharp::SymbolKind kind) {
    switch (kind) { case luasharp::SymbolKind::Class: return 0; case luasharp::SymbolKind::Enum: case luasharp::SymbolKind::Type: return 1; case luasharp::SymbolKind::Function: return 2; case luasharp::SymbolKind::Method: return 3; case luasharp::SymbolKind::Field: return 4; case luasharp::SymbolKind::Parameter: return 6; default: return 5; }
}
const luasharp::Symbol* MemberAt(const luasharp::Analysis& analysis, size_t offset) {
    const auto& tokens = analysis.syntax.tokens;
    for (size_t i = 2; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        if (token.kind != luasharp::SyntaxTokenKind::Identifier || offset < token.span.offset || offset > token.span.End() || tokens[i - 1].text != ".") continue;
        const auto receiverOffset = tokens[i - 2].span.End() ? tokens[i - 2].span.End() - 1 : tokens[i - 2].span.offset;
        const auto* receiver = luasharp::FindExpression(analysis, receiverOffset);
        if (!receiver || receiver->type == luasharp::TypeTable::Unknown) return nullptr;
        return luasharp::FindMember(analysis, analysis.types.Name(receiver->type), token.text);
    }
    return nullptr;
}
bool LambdaSignature(const std::string& type, size_t& arity, std::string& result) {
    if (!type.starts_with("$function:")) return false;
    const auto separator = type.find(':', 10); if (separator == std::string::npos) return false;
    try { arity = static_cast<size_t>(std::stoull(type.substr(10, separator - 10))); } catch (...) { return false; }
    result = type.substr(separator + 1); return true;
}
std::string DisplayType(const std::string& type) {
    size_t arity{}; std::string result;
    if (!LambdaSignature(type, arity, result)) return type;
    std::string display = "function("; for (size_t i{}; i < arity; ++i) { if (i) display += ", "; display += "any"; }
    return display + ") -> " + result;
}
std::string HoverLabel(const luasharp::Symbol& symbol) {
    if (symbol.kind == luasharp::SymbolKind::Class) return "class " + symbol.name;
    if (symbol.kind == luasharp::SymbolKind::Enum) return "enum " + symbol.name;
    if (symbol.kind != luasharp::SymbolKind::Method && symbol.kind != luasharp::SymbolKind::Function) return DisplayType(symbol.type) + " " + symbol.name;
    std::string result = symbol.type + " " + symbol.name + "(";
    for (size_t i{}; i < symbol.parameters.size(); ++i) { if (i) result += ", "; result += symbol.parameters[i]; }
    return result + ")";
}
const luasharp::Symbol* SymbolAt(const luasharp::Analysis& analysis, size_t offset) {
    const auto declaration = std::find_if(analysis.symbols.begin(), analysis.symbols.end(), [&](const auto& symbol) { return !symbol.implicit && symbol.span.Contains(offset); });
    if (declaration != analysis.symbols.end()) return &*declaration;
    if (const auto* member = MemberAt(analysis, offset)) return member;
    if (const auto* expression = luasharp::FindExpression(analysis, offset); expression && expression->symbolIndex && *expression->symbolIndex < analysis.symbols.size()) return &analysis.symbols[*expression->symbolIndex];
    const auto token = std::find_if(analysis.syntax.tokens.begin(), analysis.syntax.tokens.end(), [&](const auto& value) { return value.kind == luasharp::SyntaxTokenKind::Identifier && value.span.Contains(offset); });
    if (token != analysis.syntax.tokens.end()) { const auto type = std::find_if(analysis.symbols.begin(), analysis.symbols.end(), [&](const auto& value) { return (value.kind == luasharp::SymbolKind::Class || value.kind == luasharp::SymbolKind::Enum) && value.name == token->text; }); if (type != analysis.symbols.end()) return &*type; }
    return nullptr;
}
bool SameSymbol(const luasharp::Symbol& left, const luasharp::Symbol& right) {
    if (left.kind != right.kind || left.name != right.name) return false;
    if (left.kind == luasharp::SymbolKind::Class || left.kind == luasharp::SymbolKind::Enum) return true;
    if (left.kind == luasharp::SymbolKind::Method || left.kind == luasharp::SymbolKind::Field) return left.owner == right.owner;
    return left.span.offset == right.span.offset && left.span.length == right.span.length;
}
json SpanRange(const std::string& text, luasharp::TextSpan span) { return {{"start", PositionAt(text, span.offset)}, {"end", PositionAt(text, span.End())}}; }
json Location(const std::string& uri, const std::string& text, const luasharp::Symbol& symbol) {
    return {{"uri", uri}, {"range", SpanRange(text, symbol.span)}};
}
std::optional<json> ExternalLocation(const std::string& currentUri, const luasharp::Symbol& wanted) {
    std::optional<json> result;
    ForEachDocument([&](const std::string& uri, Document& document) {
        if (result || uri == currentUri) return; const auto& analysis = Standalone(document);
        const auto found = std::find_if(analysis.symbols.begin(), analysis.symbols.end(), [&](const auto& candidate) {
            if (!candidate.span.length || candidate.kind != wanted.kind || candidate.name != wanted.name) return false;
            return wanted.kind == luasharp::SymbolKind::Class || wanted.kind == luasharp::SymbolKind::Enum || candidate.owner == wanted.owner;
        });
        if (found != analysis.symbols.end()) result = Location(uri, document.text, *found);
    });
    return result;
}
std::vector<luasharp::TextSpan> ReferenceSpans(const luasharp::Analysis& analysis, const luasharp::Symbol& wanted) {
    std::vector<luasharp::TextSpan> result;
    for (const auto& token : analysis.syntax.tokens) {
        // Symbol spans cover only the name, so a reference token always spells it; skip the costly lookup otherwise.
        if (token.kind != luasharp::SyntaxTokenKind::Identifier || token.text != wanted.name) continue;
        const auto* symbol = SymbolAt(analysis, token.span.offset);
        if (symbol ? SameSymbol(*symbol, wanted) : wanted.kind == luasharp::SymbolKind::Class || wanted.kind == luasharp::SymbolKind::Enum) result.push_back(token.span);
    }
    return result;
}
// Calls visit(uri, text, spans) for each document with references to wanted.
template <typename Visit> void ForEachReference(const std::string& uri, const luasharp::Symbol& wanted, bool workspaceWide, Visit&& visit) {
    ForEachDocument([&](const std::string& documentUri, Document& document) {
        if (!workspaceWide && documentUri != uri) return;
        if (document.text.find(wanted.name) == std::string::npos) return; // a reference token spells the name
        const auto spans = ReferenceSpans(Editor(documentUri, document), wanted);
        if (!spans.empty()) visit(documentUri, document.text, spans);
    });
}
void PublishDiagnostics(const std::string& uri) {
    json diagnostics = json::array();
    const auto& analysis = Editor(uri); const auto& text = documents[uri].text;
    for (const auto& diagnostic : analysis.diagnostics) {
        const auto start = PositionAt(text, diagnostic.span.offset);
        const auto end = PositionAt(text, diagnostic.span.End() > diagnostic.span.offset ? diagnostic.span.End() : diagnostic.span.offset + 1);
        diagnostics.push_back({{"severity", diagnostic.severity == luasharp::DiagnosticSeverity::Error ? 1 : 2}, {"message", diagnostic.message}, {"source", "LuaSharp"}, {"range", {{"start", start}, {"end", end}}}});
    }
    Send({{"jsonrpc", "2.0"}, {"method", "textDocument/publishDiagnostics"}, {"params", {{"uri", uri}, {"diagnostics", diagnostics}}}});
}
void PublishOpenDiagnostics() { for (const auto& [uri, document] : documents) PublishDiagnostics(uri); }
json Completions(const std::string& uri, const std::string& text, const json& position) {
    const auto& analysis = Editor(uri); const auto offset = OffsetAt(text, position); const auto lineStart = offset == 0 ? 0 : text.rfind('\n', offset - 1) == std::string::npos ? 0 : text.rfind('\n', offset - 1) + 1; const auto lineEnd = text.find('\n', offset); const auto line = text.substr(lineStart, lineEnd == std::string::npos ? std::string::npos : lineEnd - lineStart); const auto character = offset - lineStart;
    json result = json::array();
    size_t memberStart = character; while (memberStart > 0 && (std::isalnum(static_cast<unsigned char>(line[memberStart - 1])) || line[memberStart - 1] == '_')) --memberStart;
    if (memberStart > 0 && line[memberStart - 1] == '.') {
        const size_t dotOffset = lineStart + memberStart - 1;
        std::optional<std::string> type;
        if (dotOffset > 0) if (const auto* expression = luasharp::FindExpression(analysis, dotOffset - 1); expression && expression->type != luasharp::TypeTable::Unknown) type = analysis.types.Name(expression->type);
        if (!type) {
            size_t end = memberStart - 1, begin = end; while (begin > 0 && (std::isalnum(static_cast<unsigned char>(line[begin - 1])) || line[begin - 1] == '_')) --begin;
            type = luasharp::ResolveVariableType(analysis, line.substr(begin, end - begin), dotOffset);
        }
        if (type) {
            const auto members = analysis.members.find(*type);
            if (members != analysis.members.end()) for (const auto index : members->second) {
                const auto& member = analysis.symbols[index];
                const auto signature = member.kind == luasharp::SymbolKind::Method ? "(" + [&] { std::string value; for (size_t i{}; i < member.parameters.size(); ++i) { if (i) value += ", "; value += member.parameters[i]; } return value; }() + ")" : "";
                json item{{"label", member.name + signature}, {"kind", member.kind == luasharp::SymbolKind::Method ? 2 : 5}, {"detail", member.type + signature}};
                if (member.kind == luasharp::SymbolKind::Method) {
                    std::string insert = member.name + "("; for (size_t i{}; i < member.parameters.size(); ++i) { if (i) insert += ", "; auto nameAt = member.parameters[i].find(' '); auto name = nameAt == std::string::npos ? "value" : member.parameters[i].substr(nameAt + 1); if (name.ends_with('?')) name.pop_back(); insert += "${" + std::to_string(i + 1) + ":" + name + "}"; } insert += ")";
                    item["insertTextFormat"] = 2; item["insertText"] = insert;
                }
                result.push_back(std::move(item));
            }
        }
        return result;
    }
    const char* words[] = {"class", "struct", "static", "enum", "public", "private", "auto", "new", "if", "else", "while", "for", "in", "break", "continue", "return", "void", "null"};
    for (const auto* word : words) result.push_back({{"label", word}, {"kind", 14}});
    for (const auto& type : luasharp::BuiltinTypes()) result.push_back({{"label", type}, {"kind", 7}, {"detail", "built-in type"}});
    for (const auto& function : luasharp::BuiltinFunctions()) {
        std::string signature = "(";
        for (size_t i{}; i < function.parameters.size(); ++i) { if (i) signature += ", "; signature += function.parameters[i].type + " " + function.parameters[i].name; }
        if (function.variadic) { if (!function.parameters.empty()) signature += ", "; signature += "any values..."; }
        signature += ")";
        result.push_back({{"label", function.name + signature}, {"kind", 3}, {"detail", function.returnType + signature}});
    }
    for (const auto& symbol : analysis.symbols) if (!symbol.implicit) result.push_back({{"label", symbol.name}, {"kind", symbol.kind == luasharp::SymbolKind::Class || symbol.kind == luasharp::SymbolKind::Enum ? 7 : symbol.kind == luasharp::SymbolKind::Method ? 2 : 6}, {"detail", DisplayType(symbol.type)}});
    return result;
}
json SignatureHelp(const std::string& uri, const std::string& text, const json& position) {
    const auto& analysis = Editor(uri); const auto cursor = OffsetAt(text, position); const auto& tokens = analysis.syntax.tokens;
    size_t limit{}; while (limit < tokens.size() && tokens[limit].span.offset < cursor) ++limit;
    size_t open = tokens.size(), depth{};
    for (size_t i = limit; i-- > 0;) { if (tokens[i].text == ")") ++depth; else if (tokens[i].text == "(") { if (depth == 0) { open = i; break; } --depth; } }
    if (open < 1 || tokens[open - 1].kind != luasharp::SyntaxTokenKind::Identifier) return nullptr;
    const auto name = tokens[open - 1].text; std::string returnType; std::vector<std::string> parameterLabels;
    if (open >= 3 && tokens[open - 2].text == ".") {
        const auto* receiver = luasharp::FindExpression(analysis, tokens[open - 3].span.End() ? tokens[open - 3].span.End() - 1 : tokens[open - 3].span.offset); if (!receiver || receiver->type == luasharp::TypeTable::Unknown) return nullptr;
        const auto* member = luasharp::FindMember(analysis, analysis.types.Name(receiver->type), name);
        if (!member || member->kind != luasharp::SymbolKind::Method) return nullptr; returnType = member->type; parameterLabels = member->parameters;
    } else if (open >= 2 && tokens[open - 2].text == "new") {
        const auto type = std::find_if(analysis.syntax.root.classes.begin(), analysis.syntax.root.classes.end(), [&](const auto& value) { return value.name == name; }); if (type == analysis.syntax.root.classes.end()) return nullptr;
        const auto constructor = std::find_if(type->functions.begin(), type->functions.end(), [](const auto& value) { return value.isConstructor; }); returnType = type->name;
        if (constructor != type->functions.end()) for (const auto& parameter : constructor->parameters) parameterLabels.push_back(parameter.type + " " + parameter.name);
    } else if (name == "int" || name == "float" || name == "bool" || name == "string") { returnType = name; parameterLabels = {"any value"}; }
    else if (const auto builtin = luasharp::FindBuiltinFunction(name)) {
        returnType = builtin->returnType;
        for (const auto& parameter : builtin->parameters) parameterLabels.push_back(parameter.type + " " + parameter.name);
        if (builtin->variadic) parameterLabels.push_back("any values...");
    }
    else {
        const luasharp::Symbol* callable = nullptr;
        if (const auto* expression = luasharp::FindExpression(analysis, tokens[open - 1].span.offset); expression && expression->symbolIndex && *expression->symbolIndex < analysis.symbols.size()) callable = &analysis.symbols[*expression->symbolIndex];
        if (!callable) return nullptr;
        if (callable->kind == luasharp::SymbolKind::Method) { returnType = callable->type; parameterLabels = callable->parameters; }
        else {
            size_t arity{}; if (!LambdaSignature(callable->type, arity, returnType)) return nullptr;
            for (size_t i{}; i < arity; ++i) parameterLabels.push_back("any value" + std::to_string(i + 1));
        }
    }
    std::string label = name + "("; for (size_t i{}; i < parameterLabels.size(); ++i) { if (i) label += ", "; label += parameterLabels[i]; } label += ") -> " + returnType;
    json parameters = json::array(); for (const auto& parameter : parameterLabels) parameters.push_back({{"label", parameter}});
    size_t activeParameter{}, argumentDepth{};
    for (size_t i = open + 1; i < limit; ++i) { if (tokens[i].text == "(" || tokens[i].text == "[" || tokens[i].text == "{") ++argumentDepth; else if (tokens[i].text == ")" || tokens[i].text == "]" || tokens[i].text == "}") { if (argumentDepth) --argumentDepth; } else if (tokens[i].text == "," && argumentDepth == 0) ++activeParameter; }
    return {{"signatures", json::array({{{"label", label}, {"parameters", parameters}}})}, {"activeSignature", 0}, {"activeParameter", activeParameter}};
}
// Innermost bound expression covering each token, matching FindExpression; built in one pass over the pool.
std::vector<const luasharp::BoundExpression*> InnermostExpressions(const luasharp::Analysis& analysis) {
    const auto& tokens = analysis.syntax.tokens;
    std::vector<const luasharp::BoundExpression*> result(tokens.size());
    const auto byOffset = [](const luasharp::SyntaxToken& token, size_t offset) { return token.span.offset < offset; };
    for (const auto& expression : analysis.bound.expressions) {
        if (!expression.syntax) continue;
        // A token is covered when TextSpan::Contains its start; children follow their parent, so <= keeps the innermost.
        auto i = static_cast<size_t>(std::lower_bound(tokens.begin(), tokens.end(), expression.span.offset, byOffset) - tokens.begin());
        for (; i < tokens.size() && expression.span.Contains(tokens[i].span.offset); ++i)
            if (!result[i] || expression.span.length <= result[i]->span.length) result[i] = &expression;
    }
    return result;
}
// Converts increasing offsets to LSP positions without rescanning the text.
class PositionCursor {
public:
    explicit PositionCursor(const std::string& text) : text_(text) {}
    std::pair<size_t, size_t> Advance(size_t offset) {
        assert(offset >= at_ && offset <= text_.size());
        while (at_ < offset) {
            const auto lead = static_cast<unsigned char>(text_[at_]);
            if (lead == '\n') { ++line_; character_ = 0; ++at_; continue; }
            const size_t bytes = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
            character_ += bytes == 4 ? 2 : 1; at_ = (std::min)(at_ + bytes, offset);
        }
        return {line_, character_};
    }
private:
    const std::string& text_;
    size_t at_{}, line_{}, character_{};
};
json SemanticTokens(const std::string& uri, const std::string& text) {
    const auto& analysis = Editor(uri);
    const auto& tokens = analysis.syntax.tokens;
    const auto innermost = InnermostExpressions(analysis);
    std::unordered_map<size_t, const luasharp::Symbol*> declarations; // first symbol declared at each offset
    std::unordered_set<std::string_view> typeNames;
    for (const auto& symbol : analysis.symbols) {
        declarations.try_emplace(symbol.span.offset, &symbol);
        if (symbol.kind == luasharp::SymbolKind::Class || symbol.kind == luasharp::SymbolKind::Enum) typeNames.insert(symbol.name);
    }
    const auto symbolOf = [&](const luasharp::BoundExpression* expression) -> const luasharp::Symbol* {
        return expression && expression->symbolIndex && *expression->symbolIndex < analysis.symbols.size() ? &analysis.symbols[*expression->symbolIndex] : nullptr;
    };
    // Same rule as MemberAt: a name after '.' resolves on its receiver's type.
    const auto memberAt = [&](size_t i) -> const luasharp::Symbol* {
        if (i < 2 || tokens[i - 1].text != ".") return nullptr;
        const auto* receiver = innermost[i - 2];
        if (!receiver || receiver->type == luasharp::TypeTable::Unknown) return nullptr;
        return luasharp::FindMember(analysis, analysis.types.Name(receiver->type), tokens[i].text);
    };
    json data = json::array(); size_t previousLine{}, previousCharacter{}; bool emitted = false;
    PositionCursor cursor(text);
    for (size_t i{}; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        if (token.kind != luasharp::SyntaxTokenKind::Identifier) continue;
        int tokenKind = -1;
        const luasharp::Symbol* symbol = nullptr;
        if (const auto declaration = declarations.find(token.span.offset); declaration != declarations.end()) symbol = declaration->second;
        else symbol = memberAt(i);
        if (!symbol) symbol = symbolOf(innermost[i]);
        if (symbol) tokenKind = symbol->kind == luasharp::SymbolKind::Method && symbol->name == symbol->owner ? 1 : TokenKind(symbol->kind); // a constructor name is the type
        else if (typeNames.contains(token.text) || luasharp::IsNativeStruct(token.text) || luasharp::IsBuiltinStaticClass(token.text)) tokenKind = 1;
        else if (i + 1 < tokens.size() && tokens[i + 1].text == "(" && luasharp::FindBuiltinFunction(token.text)) tokenKind = 2;
        if (tokenKind < 0) continue;
        const auto [line, character] = cursor.Advance((std::min)(token.span.offset, text.size()));
        data.push_back(emitted ? line - previousLine : line); data.push_back(emitted && line == previousLine ? character - previousCharacter : character); data.push_back(token.span.length); data.push_back(tokenKind); data.push_back(0);
        previousLine = line; previousCharacter = character; emitted = true;
    }
    return {{"data", data}};
}
}

int main() {
    while (auto message = ReadMessage()) {
        const auto method = message->value("method", "");
        if (method == "initialize") Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", {{"capabilities", {{"textDocumentSync", 2}, {"completionProvider", {{"triggerCharacters", json::array({"."})}}}, {"signatureHelpProvider", {{"triggerCharacters", json::array({"(", ","})}}}, {"hoverProvider", true}, {"definitionProvider", true}, {"referencesProvider", true}, {"renameProvider", true}, {"documentSymbolProvider", true}, {"workspaceSymbolProvider", true}, {"semanticTokensProvider", {{"legend", {{"tokenTypes", json::array({"class", "type", "function", "method", "property", "variable", "parameter"})}, {"tokenModifiers", json::array()}}}, {"full", true}}}}}}}});
        else if (method == "luasharp/indexWorkspace") { indexedDocuments.clear(); for (const auto& document : (*message)["params"].value("documents", json::array())) indexedDocuments[document.value("uri", "")].SetText(document.value("text", "")); ++exportsRevision; PublishOpenDiagnostics(); }
        // One file changed on disk. Standalone() advances exportsRevision only if its exported types differ, so other files stay cached otherwise.
        else if (method == "luasharp/indexDocument") { const auto& p = (*message)["params"]; indexedDocuments[p.value("uri", "")].SetText(p.value("text", "")); PublishOpenDiagnostics(); }
        else if (method == "luasharp/removeDocument") { if (indexedDocuments.erase((*message)["params"].value("uri", ""))) { ++exportsRevision; PublishOpenDiagnostics(); } }
        else if (method == "textDocument/didOpen") { UpdateDocument((*message)["params"], true); PublishDiagnostics((*message)["params"]["textDocument"].value("uri", "")); }
        else if (method == "textDocument/didChange") { UpdateDocument((*message)["params"], false); PublishDiagnostics((*message)["params"]["textDocument"].value("uri", "")); }
        else if (method == "textDocument/didClose") { const auto uri = (*message)["params"]["textDocument"].value("uri", ""); documents.erase(uri); ++exportsRevision; Send({{"jsonrpc", "2.0"}, {"method", "textDocument/publishDiagnostics"}, {"params", {{"uri", uri}, {"diagnostics", json::array()}}}}); }
        else if (method == "textDocument/completion") { const auto& p = (*message)["params"]; const auto uri = p["textDocument"].value("uri", ""); Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", Completions(uri, documents[uri].text, p["position"])}}); }
        else if (method == "textDocument/signatureHelp") { const auto& p = (*message)["params"]; const auto uri = p["textDocument"].value("uri", ""); Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", SignatureHelp(uri, documents[uri].text, p["position"])}}); }
        else if (method == "textDocument/semanticTokens/full") { const auto& p = (*message)["params"]; const auto uri = p["textDocument"].value("uri", ""); Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", SemanticTokens(uri, documents[uri].text)}}); }
        else if (method == "textDocument/references" || method == "textDocument/rename") {
            const auto& p = (*message)["params"]; const auto uri = p["textDocument"].value("uri", ""); const auto& text = documents[uri].text; const auto* found = SymbolAt(Editor(uri), OffsetAt(text, p["position"]));
            const std::optional<luasharp::Symbol> symbol = found ? std::optional(*found) : std::nullopt;
            const bool workspaceWide = symbol && (symbol->kind == luasharp::SymbolKind::Class || symbol->kind == luasharp::SymbolKind::Enum || symbol->kind == luasharp::SymbolKind::Method || symbol->kind == luasharp::SymbolKind::Field);
            if (method == "textDocument/references") {
                json locations = json::array(); if (symbol) ForEachReference(uri, *symbol, workspaceWide, [&](const std::string& documentUri, const std::string& documentText, const std::vector<luasharp::TextSpan>& spans) { for (const auto span : spans) locations.push_back({{"uri", documentUri}, {"range", SpanRange(documentText, span)}}); });
                Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", locations}});
            } else {
                json changes = json::object(); if (symbol && (symbol->span.length || workspaceWide)) ForEachReference(uri, *symbol, workspaceWide, [&](const std::string& documentUri, const std::string& documentText, const std::vector<luasharp::TextSpan>& spans) { changes[documentUri] = json::array(); for (const auto span : spans) changes[documentUri].push_back({{"range", SpanRange(documentText, span)}, {"newText", p.value("newName", symbol->name)}}); });
                Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", {{"changes", changes}}}});
            }
        }
        else if (method == "textDocument/documentSymbol") {
            const auto& p = (*message)["params"]; const auto uri = p["textDocument"].value("uri", ""); const auto& text = documents[uri].text; const auto& analysis = Editor(uri); json symbols = json::array();
            for (const auto& symbol : analysis.symbols) { if (symbol.implicit || symbol.kind == luasharp::SymbolKind::Variable || symbol.kind == luasharp::SymbolKind::Parameter) continue; const int kind = symbol.kind == luasharp::SymbolKind::Class ? 5 : symbol.kind == luasharp::SymbolKind::Enum ? 10 : symbol.kind == luasharp::SymbolKind::Method ? 6 : 8; symbols.push_back({{"name", symbol.name}, {"kind", kind}, {"range", SpanRange(text, symbol.span)}, {"selectionRange", SpanRange(text, symbol.span)}}); }
            Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", symbols}});
        }
        else if (method == "workspace/symbol") {
            const auto query = (*message)["params"].value("query", ""); json symbols = json::array();
            ForEachDocument([&](const std::string& uri, Document& document) { for (const auto& symbol : Standalone(document).symbols) { if (symbol.implicit) continue; if (symbol.kind != luasharp::SymbolKind::Class && symbol.kind != luasharp::SymbolKind::Enum && symbol.kind != luasharp::SymbolKind::Method && symbol.kind != luasharp::SymbolKind::Field) continue; if (!query.empty() && symbol.name.find(query) == std::string::npos) continue; const int kind = symbol.kind == luasharp::SymbolKind::Class ? 5 : symbol.kind == luasharp::SymbolKind::Enum ? 10 : symbol.kind == luasharp::SymbolKind::Method ? 6 : 8; symbols.push_back({{"name", symbol.name}, {"kind", kind}, {"location", Location(uri, document.text, symbol)}}); } });
            Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", symbols}});
        }
        else if (method == "textDocument/hover" || method == "textDocument/definition") {
            const auto& p = (*message)["params"]; const auto uri = p["textDocument"].value("uri", ""); const auto& text = documents[uri].text; const auto& analysis = Editor(uri); const auto offset = OffsetAt(text, p["position"]);
            const luasharp::Symbol* symbol = MemberAt(analysis, offset);
            if (const auto* expression = luasharp::FindExpression(analysis, offset); expression && expression->symbolIndex && *expression->symbolIndex < analysis.symbols.size()) symbol = &analysis.symbols[*expression->symbolIndex];
            if (!symbol) { const auto word = WordAt(text, p["position"]); const auto found = std::find_if(analysis.symbols.begin(), analysis.symbols.end(), [&](const auto& candidate) { return !candidate.implicit && candidate.name == word; }); if (found != analysis.symbols.end()) symbol = &*found; }
            if (method == "textDocument/definition") { json locations = json::array(); if (symbol && symbol->span.length) locations.push_back(Location(uri, text, *symbol)); else if (symbol) if (const auto external = ExternalLocation(uri, *symbol)) locations.push_back(*external); Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", locations}}); }
            else Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", symbol ? json{{"contents", {{"kind", "markdown"}, {"value", "```luasharp\n" + HoverLabel(*symbol) + "\n```"}}}} : json(nullptr)}});
        } else if (method == "shutdown") Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", nullptr}});
        else if (method == "exit") break;
    }
}
