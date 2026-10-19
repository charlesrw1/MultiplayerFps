#include "Compiler.h"
#include "Builtins.h"
#include "Project.h"
#include "Semantic.h"

#include <windows.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
// Analyses are cached per document. A text change drops only that document's
// analyses; other documents re-analyze only when its exported types change.
struct Document {
    std::string text;
    std::unique_ptr<luasharp::Analysis> standalone; // without other files' types; source of exports
    std::vector<luasharp::ExternalType> exports;    // types this file offers the others
    std::unique_ptr<luasharp::Analysis> editor;     // with the other files' exports
    uint64_t editorRevision{};                      // exportsRevision the editor analysis was built against
    std::string pathKey;                            // luasharp::PathKey of its file; empty for a non-file uri
    std::string project;                            // PathKey of its luasharp.json; empty outside a project
    bool declaration{};                             // one of its project's declaration files
    fs::file_time_type stamp{};                     // declaration files: write time of the text read from disk

    void SetText(std::string value) { text = std::move(value); standalone.reset(); editor.reset(); }
};
std::unordered_map<std::string, Document> documents;        // open in the editor
std::unordered_map<std::string, Document> indexedDocuments; // workspace files; open documents win
uint64_t exportsRevision = 1;                               // advances when the set of exported types changes

struct ProjectEntry {
    fs::path file;                         // luasharp.json
    fs::file_time_type stamp{(fs::file_time_type::min)()};
    std::vector<std::string> declarationUris;
};
std::unordered_map<std::string, ProjectEntry> projects;          // by PathKey of luasharp.json
std::unordered_map<std::string, Document> declarationDocuments; // read from disk, by uri

template <typename Visit> void ForEachDocument(Visit&& visit) {
    for (auto& [uri, document] : documents) visit(uri, document);
    for (auto& [uri, document] : indexedDocuments) if (!documents.contains(uri)) visit(uri, document);
}

std::string PercentDecode(std::string_view text) {
    std::string result;
    for (size_t i{}; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size() && std::isxdigit(static_cast<unsigned char>(text[i + 1])) && std::isxdigit(static_cast<unsigned char>(text[i + 2]))) {
            result += static_cast<char>(std::stoi(std::string(text.substr(i + 1, 2)), nullptr, 16)); i += 2;
        } else result += text[i];
    }
    return result;
}
std::optional<fs::path> UriToPath(const std::string& uri) {
    if (!uri.starts_with("file://")) return std::nullopt;
    auto path = PercentDecode(std::string_view(uri).substr(7));
    if (path.size() > 2 && path[0] == '/' && path[2] == ':') path.erase(0, 1); // file:///c:/x
    return fs::path(std::u8string(path.begin(), path.end()));
}
std::string PathToUri(const fs::path& path) {
    static const char* hex = "0123456789ABCDEF";
    std::string result = "file:///";
    for (const char8_t c : fs::absolute(path).generic_u8string()) {
        if (std::isalnum(c) || std::string_view("/:-._~").find(static_cast<char>(c)) != std::string_view::npos) result += static_cast<char>(c);
        else { result += '%'; result += hex[c >> 4]; result += hex[c & 15]; }
    }
    return result;
}
fs::file_time_type WriteTime(const fs::path& path) {
    std::error_code ec; const auto time = fs::last_write_time(path, ec);
    return ec ? (fs::file_time_type::min)() : time;
}
std::string ReadText(const fs::path& path) {
    std::ifstream file(path, std::ios::binary); std::ostringstream text; text << file.rdbuf(); return text.str();
}

// Reloads the declaration list of a project and the declaration files changed on disk. Errors are logged when
// luasharp.json changes; on a malformed file the previous declarations stay.
void SyncProject(const std::string& key, ProjectEntry& entry) {
    const auto stamp = WriteTime(entry.file);
    const bool changed = stamp != entry.stamp; entry.stamp = stamp;
    luasharp::Project project;
    try { project = luasharp::LoadProject(entry.file); }
    catch (const std::exception& error) { if (changed) std::cerr << "luasharp: " << error.what() << '\n'; return; }
    if (changed) for (const auto& missing : project.missing) std::cerr << "luasharp: " << entry.file.string() << ": declaration path '" << missing << "' does not exist\n";
    std::vector<std::string> uris;
    for (const auto& file : project.declarationFiles) {
        auto& document = declarationDocuments[uris.emplace_back(PathToUri(file))];
        const auto fileStamp = WriteTime(file);
        if (!document.pathKey.empty() && document.stamp == fileStamp) continue;
        document.SetText(ReadText(file)); // Standalone() advances exportsRevision if the types differ
        document.stamp = fileStamp; document.pathKey = luasharp::PathKey(file); document.project = key; document.declaration = true;
    }
    for (const auto& uri : entry.declarationUris)
        if (std::find(uris.begin(), uris.end(), uri) == uris.end()) { declarationDocuments.erase(uri); ++exportsRevision; }
    entry.declarationUris = std::move(uris);
}
void SyncProjects() { for (auto& [key, entry] : projects) SyncProject(key, entry); }

// Finds the project of a document, and whether the document is one of its declaration files.
void Place(const std::string& uri, Document& document) {
    document.pathKey.clear(); document.project.clear(); document.declaration = false;
    const auto path = UriToPath(uri); if (!path) return;
    document.pathKey = luasharp::PathKey(*path);
    const auto projectFile = luasharp::FindProjectFile(*path); if (!projectFile) return;
    document.project = luasharp::PathKey(*projectFile);
    const auto [entry, added] = projects.try_emplace(document.project, ProjectEntry{*projectFile});
    if (added) SyncProject(document.project, entry->second);
    for (const auto& declarationUri : entry->second.declarationUris) document.declaration = document.declaration || declarationDocuments[declarationUri].pathKey == document.pathKey;
}

// Calls visit(uri, document) for each document whose types `from` sees: the declaration files of its project, then
// (unless `from` is one of them) the other files of that project. Files outside a project see each other.
template <typename Visit> void ForEachVisible(const Document& from, Visit&& visit) {
    if (!from.project.empty())
        for (const auto& uri : projects[from.project].declarationUris) if (auto& document = declarationDocuments[uri]; document.pathKey != from.pathKey) visit(uri, document);
    if (from.declaration) return;
    ForEachDocument([&](const std::string& uri, Document& document) { if (&document != &from && !document.declaration && document.project == from.project) visit(uri, document); });
}

// Positions are ignored: moving a declaration does not change what other files see.
bool SameExports(const std::vector<luasharp::ExternalType>& left, const std::vector<luasharp::ExternalType>& right) {
    const auto sameMember = [](const luasharp::Symbol& a, const luasharp::Symbol& b) { return a.name == b.name && a.type == b.type && a.kind == b.kind && a.parameters == b.parameters && a.owner == b.owner && a.access == b.access && a.isStatic == b.isStatic && a.typeParameter == b.typeParameter && a.isTypeOf == b.isTypeOf; };
    const auto sameType = [&](const luasharp::ExternalType& a, const luasharp::ExternalType& b) { return a.name == b.name && a.baseType == b.baseType && a.isEnum == b.isEnum && a.isNative == b.isNative && a.hostDefault == b.hostDefault && std::equal(a.members.begin(), a.members.end(), b.members.begin(), b.members.end(), sameMember); };
    return std::equal(left.begin(), left.end(), right.begin(), right.end(), sameType);
}
const luasharp::Analysis& Standalone(Document& document) {
    if (document.standalone) return *document.standalone;
    document.standalone = std::make_unique<luasharp::Analysis>(luasharp::Analyze(document.text));
    auto exports = luasharp::ExportTypes(*document.standalone);
    if (!SameExports(exports, document.exports)) { document.exports = std::move(exports); ++exportsRevision; }
    return *document.standalone;
}
std::vector<luasharp::ExternalType> VisibleExports(Document& document) {
    std::vector<luasharp::ExternalType> external;
    ForEachVisible(document, [&](const std::string&, Document& value) { external.insert(external.end(), value.exports.begin(), value.exports.end()); });
    return external;
}
// Refreshes every document's exports first, so exportsRevision is stable for the
// rest of the request and earlier references from Editor() stay valid.
const luasharp::Analysis& Editor(Document& document) {
    ForEachDocument([](const std::string&, Document& value) { Standalone(value); });
    for (auto& [uri, value] : declarationDocuments) Standalone(value);
    if (document.editor && document.editorRevision == exportsRevision) return *document.editor;
    document.editor = std::make_unique<luasharp::Analysis>(luasharp::Analyze(document.text, VisibleExports(document)));
    document.editorRevision = exportsRevision;
    return *document.editor;
}
const luasharp::Analysis& Editor(const std::string& uri) { return Editor(documents[uri]); }

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
    if (opened) { documents[uri].SetText(document.value("text", "")); Place(uri, documents[uri]); ++exportsRevision; return; }
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
// Indices into the semantic token legend sent on initialize.
constexpr int ClassTokenKind = 0, StructTokenKind = 7;
int TokenKind(const luasharp::Symbol& symbol) {
    switch (symbol.kind) { case luasharp::SymbolKind::Class: return symbol.isStruct ? StructTokenKind : ClassTokenKind; case luasharp::SymbolKind::Enum: case luasharp::SymbolKind::Type: return 1; case luasharp::SymbolKind::Function: return 2; case luasharp::SymbolKind::Method: return 3; case luasharp::SymbolKind::Field: return 4; case luasharp::SymbolKind::Parameter: return 6; default: return 5; }
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
bool FunctionSignature(const luasharp::Analysis& analysis, const std::string& type, std::vector<std::string>& parameters, std::string& result) {
    const auto id = analysis.types.Find(type);
    if (!id || !analysis.types.IsFunction(*id)) return false;
    for (const auto parameter : analysis.types.Parameters(*id)) parameters.push_back(analysis.types.Name(parameter));
    result = analysis.types.Name(analysis.types.Element(*id));
    return true;
}
std::string JoinLabels(std::span<const std::string> labels) {
    std::string result; for (size_t i{}; i < labels.size(); ++i) { if (i) result += ", "; result += labels[i]; } return result;
}
// `name<T : Base>(params)` for a generic method, else `name(params)`.
std::string MethodSignature(const luasharp::Symbol& method) {
    const auto generic = method.typeParameter.empty() ? std::string() : "<" + method.typeParameter + " : " + method.type + ">";
    return method.name + generic + "(" + JoinLabels(luasharp::ExplicitParameters(method)) + ")";
}
std::string ResultType(const luasharp::Symbol& method) { return method.typeParameter.empty() ? method.type : method.typeParameter; }
std::string HoverLabel(const luasharp::Symbol& symbol) {
    if (symbol.kind == luasharp::SymbolKind::Class) return "class " + symbol.name;
    if (symbol.kind == luasharp::SymbolKind::Enum) return "enum " + symbol.name;
    if (symbol.kind != luasharp::SymbolKind::Method && symbol.kind != luasharp::SymbolKind::Function) return symbol.type + " " + symbol.name;
    return ResultType(symbol) + " " + MethodSignature(symbol);
}
// The symbol an expression names. A `typeof(T)` or `f<T>` type argument names the type T, not the [TypeOf] method it calls.
const luasharp::Symbol* ExpressionSymbol(const luasharp::Analysis& analysis, const luasharp::BoundExpression* expression) {
    if (!expression || !expression->symbolIndex || *expression->symbolIndex >= analysis.symbols.size()) return nullptr;
    if (expression->syntax && expression->syntax->kind == luasharp::SyntaxExpression::Kind::TypeOf) return nullptr;
    return &analysis.symbols[*expression->symbolIndex];
}
const luasharp::Symbol* SymbolAt(const luasharp::Analysis& analysis, size_t offset) {
    const auto declaration = std::find_if(analysis.symbols.begin(), analysis.symbols.end(), [&](const auto& symbol) { return !symbol.implicit && symbol.span.Contains(offset); });
    if (declaration != analysis.symbols.end()) return &*declaration;
    if (const auto* member = MemberAt(analysis, offset)) return member;
    if (const auto* symbol = ExpressionSymbol(analysis, luasharp::FindExpression(analysis, offset))) return symbol;
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
std::optional<json> ExternalLocation(const Document& from, const luasharp::Symbol& wanted) {
    std::optional<json> result;
    ForEachVisible(from, [&](const std::string& uri, Document& document) {
        if (result) return; const auto& analysis = Standalone(document);
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
// Calls visit(uri, text, spans) for each document of the same project with references to wanted. Declaration files
// are generated, so they are not visited.
template <typename Visit> void ForEachReference(const std::string& uri, const luasharp::Symbol& wanted, bool workspaceWide, Visit&& visit) {
    const auto& project = documents[uri].project;
    ForEachDocument([&](const std::string& documentUri, Document& document) {
        if (!workspaceWide && documentUri != uri) return;
        if (document.project != project || (document.declaration && documentUri != uri)) return;
        if (document.text.find(wanted.name) == std::string::npos) return; // a reference token spells the name
        const auto spans = ReferenceSpans(Editor(document), wanted);
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
json MemberCompletion(const luasharp::Symbol& member) {
    if (member.kind != luasharp::SymbolKind::Method) return {{"label", member.name}, {"kind", 5}, {"detail", member.type}};
    const auto signature = MethodSignature(member);
    json item{{"label", signature}, {"kind", 2}, {"detail", ResultType(member) + " " + signature}};
    // A generic method's snippet starts with its type argument, the constraint by default.
    size_t placeholder{};
    const auto next = [&](std::string_view text) { return "${" + std::to_string(++placeholder) + ":" + std::string(text) + "}"; };
    std::string insert = member.name;
    if (!member.typeParameter.empty()) insert += "<" + next(member.type) + ">";
    insert += "(";
    const auto parameters = luasharp::ExplicitParameters(member);
    for (size_t i{}; i < parameters.size(); ++i) { if (i) insert += ", "; const auto label = luasharp::SplitParameterLabel(parameters[i]); insert += next(label.name.empty() ? "value" : label.name); }
    item["insertTextFormat"] = 2; item["insertText"] = insert + ")";
    return item;
}
// Members reachable without `this.`: statics always, instance members only outside static functions.
void AddEnclosingClassCompletions(const luasharp::Analysis& analysis, size_t offset, json& result) {
    const auto& classes = analysis.syntax.root.classes;
    const auto enclosing = std::find_if(classes.begin(), classes.end(), [&](const auto& value) { return value.bodySpan.Contains(offset); });
    if (enclosing == classes.end()) return;
    const auto function = std::find_if(enclosing->functions.begin(), enclosing->functions.end(), [&](const auto& value) { return value.bodySpan.Contains(offset); });
    const bool staticContext = function != enclosing->functions.end() && function->isStatic;
    const auto add = [&](const std::string& key) {
        const auto members = analysis.members.find(key); if (members == analysis.members.end()) return;
        for (const auto index : members->second) {
            const auto& member = analysis.symbols[index];
            if (member.access == "private" && member.owner != enclosing->name) continue;
            if (!staticContext || member.isStatic) result.push_back(MemberCompletion(member));
        }
    };
    add(enclosing->name); add("$type:" + enclosing->name);
}
// A half-typed member (`f().`, `f().co`) breaks the statement, so the receiver has no type. This analyzes a copy
// that ends the member as a complete statement.
luasharp::Analysis CompletedMemberAnalysis(Document& document, size_t dotOffset, size_t cursor) {
    assert(dotOffset < cursor && cursor <= document.text.size());
    auto text = document.text;
    text.replace(dotOffset + 1, cursor - dotOffset - 1, "ls_completion;");
    return luasharp::Analyze(text, VisibleExports(document));
}
json Completions(const std::string& uri, const std::string& text, const json& position) {
    const auto& analysis = Editor(uri); const auto offset = OffsetAt(text, position); const auto lineStart = offset == 0 ? 0 : text.rfind('\n', offset - 1) == std::string::npos ? 0 : text.rfind('\n', offset - 1) + 1; const auto lineEnd = text.find('\n', offset); const auto line = text.substr(lineStart, lineEnd == std::string::npos ? std::string::npos : lineEnd - lineStart); const auto character = offset - lineStart;
    json result = json::array();
    size_t memberStart = character; while (memberStart > 0 && (std::isalnum(static_cast<unsigned char>(line[memberStart - 1])) || line[memberStart - 1] == '_')) --memberStart;
    if (memberStart > 0 && line[memberStart - 1] == '.') {
        const size_t dotOffset = lineStart + memberStart - 1;
        // Members come from the same analysis as the type: a broken statement can also drop the class's members.
        const auto addMembers = [&](const luasharp::Analysis& value, const std::optional<std::string>& type) {
            if (!type) return;
            const auto members = value.members.find(*type);
            if (members != value.members.end()) for (const auto index : members->second) result.push_back(MemberCompletion(value.symbols[index]));
        };
        // The receiver is the first child of the innermost member access over the dot: a call's span stops before
        // its '(', so the expression at the character before the dot can be the member access itself.
        const auto typeBefore = [&](const luasharp::Analysis& value) -> std::optional<std::string> {
            const luasharp::BoundExpression* access = nullptr;
            for (const auto& expression : value.bound.expressions)
                if (expression.syntax && expression.syntax->kind == luasharp::SyntaxExpression::Kind::Member && expression.children.count && expression.span.Contains(dotOffset) && (!access || expression.span.length <= access->span.length)) access = &expression;
            const auto* receiver = access ? &value.bound.expressions[access->children.first] : dotOffset > 0 ? luasharp::FindExpression(value, dotOffset - 1) : nullptr;
            if (!receiver || receiver->type == luasharp::TypeTable::Unknown) return std::nullopt;
            return value.types.Name(receiver->type);
        };
        addMembers(analysis, typeBefore(analysis));
        if (!result.empty()) return result;
        const auto completed = CompletedMemberAnalysis(documents[uri], dotOffset, offset);
        addMembers(completed, typeBefore(completed));
        if (!result.empty()) return result;
        size_t end = memberStart - 1, begin = end; while (begin > 0 && (std::isalnum(static_cast<unsigned char>(line[begin - 1])) || line[begin - 1] == '_')) --begin;
        addMembers(analysis, luasharp::ResolveVariableType(analysis, line.substr(begin, end - begin), dotOffset));
        return result;
    }
    const char* words[] = {"class", "struct", "static", "enum", "public", "private", "auto", "new", "if", "else", "while", "for", "in", "break", "continue", "return", "void", "null",
                           "virtual", "override", "abstract", "sealed", "extern", "inout", "is", "as", "typeof", "base"};
    for (const auto* word : words) result.push_back({{"label", word}, {"kind", 14}});
    for (const auto& type : luasharp::BuiltinTypes()) result.push_back({{"label", type}, {"kind", 7}, {"detail", "built-in type"}});
    for (const auto& type : luasharp::BuiltinStaticClasses()) result.push_back({{"label", type}, {"kind", 7}, {"detail", "built-in static class"}});
    for (const auto& function : luasharp::BuiltinFunctions()) {
        std::string signature = "(";
        for (size_t i{}; i < function.parameters.size(); ++i) { if (i) signature += ", "; signature += function.parameters[i].type + " " + function.parameters[i].name; }
        if (function.variadic) { if (!function.parameters.empty()) signature += ", "; signature += "any values..."; }
        signature += ")";
        result.push_back({{"label", function.name + signature}, {"kind", 3}, {"detail", function.returnType + signature}});
    }
    AddEnclosingClassCompletions(analysis, offset, result);
    for (const auto& symbol : analysis.symbols) if (!symbol.implicit && symbol.kind != luasharp::SymbolKind::Method && symbol.kind != luasharp::SymbolKind::Field) result.push_back({{"label", symbol.name}, {"kind", symbol.kind == luasharp::SymbolKind::Class || symbol.kind == luasharp::SymbolKind::Enum ? 7 : 6}, {"detail", symbol.type}});
    return result;
}
json SignatureHelp(const std::string& uri, const std::string& text, const json& position) {
    const auto& analysis = Editor(uri); const auto cursor = OffsetAt(text, position); const auto& tokens = analysis.syntax.tokens;
    size_t limit{}; while (limit < tokens.size() && tokens[limit].span.offset < cursor) ++limit;
    size_t open = tokens.size(), depth{};
    for (size_t i = limit; i-- > 0;) { if (tokens[i].text == ")") ++depth; else if (tokens[i].text == "(") { if (depth == 0) { open = i; break; } --depth; } }
    // `f<A>(`: the name is before the type argument list.
    size_t nameIndex = open - 1; std::string typeArguments;
    if (open >= 1 && tokens[nameIndex].text == ">") {
        size_t angles{}, at = nameIndex + 1;
        while (at-- > 0) { if (tokens[at].text == ">") ++angles; else if (tokens[at].text == "<" && --angles == 0) break; }
        if (at >= nameIndex) return nullptr;
        for (size_t i = at + 1; i < nameIndex; ++i) typeArguments += tokens[i].text == "," ? ", " : tokens[i].text;
        nameIndex = at - 1;
    }
    if (open < 1 || nameIndex >= open || tokens[nameIndex].kind != luasharp::SyntaxTokenKind::Identifier) return nullptr;
    const auto name = tokens[nameIndex].text; std::string returnType; std::vector<std::string> parameterLabels;
    const auto useMethod = [&](const luasharp::Symbol& method) {
        if (method.typeParameter.empty() || typeArguments.empty()) { returnType = method.type; parameterLabels = method.parameters; return; }
        const auto parameters = luasharp::ExplicitParameters(method);
        returnType = typeArguments; parameterLabels.assign(parameters.begin(), parameters.end());
    };
    if (nameIndex >= 2 && tokens[nameIndex - 1].text == ".") {
        const auto& receiverToken = tokens[nameIndex - 2];
        const auto* receiver = luasharp::FindExpression(analysis, receiverToken.span.End() ? receiverToken.span.End() - 1 : receiverToken.span.offset); if (!receiver || receiver->type == luasharp::TypeTable::Unknown) return nullptr;
        const auto* member = luasharp::FindMember(analysis, analysis.types.Name(receiver->type), name);
        if (!member || member->kind != luasharp::SymbolKind::Method) return nullptr; useMethod(*member);
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
        const luasharp::Symbol* callable = ExpressionSymbol(analysis, luasharp::FindExpression(analysis, tokens[nameIndex].span.offset));
        if (!callable) return nullptr;
        if (callable->kind == luasharp::SymbolKind::Method) useMethod(*callable);
        else {
            std::vector<std::string> types; if (!FunctionSignature(analysis, callable->type, types, returnType)) return nullptr;
            for (size_t i{}; i < types.size(); ++i) parameterLabels.push_back(types[i] + " value" + std::to_string(i + 1));
        }
    }
    std::string label = name + (typeArguments.empty() ? "" : "<" + typeArguments + ">") + "(" + JoinLabels(parameterLabels) + ") -> " + returnType;
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
    std::unordered_map<std::string_view, int> typeKinds; // declared type name -> token kind
    for (const auto& symbol : analysis.symbols) {
        declarations.try_emplace(symbol.span.offset, &symbol);
        if (symbol.kind == luasharp::SymbolKind::Class || symbol.kind == luasharp::SymbolKind::Enum) typeKinds.try_emplace(symbol.name, TokenKind(symbol));
    }
    const auto typeKind = [&](std::string_view name) {
        if (const auto found = typeKinds.find(name); found != typeKinds.end()) return found->second;
        if (luasharp::IsNativeStruct(name)) return StructTokenKind;
        return luasharp::IsBuiltinStaticClass(name) ? ClassTokenKind : -1;
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
        if (!symbol) symbol = ExpressionSymbol(analysis, innermost[i]);
        if (symbol) tokenKind = symbol->kind == luasharp::SymbolKind::Method && symbol->name == symbol->owner ? typeKind(symbol->owner) : TokenKind(*symbol); // a constructor name is the type
        else tokenKind = typeKind(token.text);
        if (tokenKind < 0 && i + 1 < tokens.size() && tokens[i + 1].text == "(" && luasharp::FindBuiltinFunction(token.text)) tokenKind = 2;
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
        SyncProjects(); // luasharp.json and the declaration files change outside the editor
        if (method == "initialize") Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", {{"capabilities", {{"textDocumentSync", 2}, {"completionProvider", {{"triggerCharacters", json::array({"."})}}}, {"signatureHelpProvider", {{"triggerCharacters", json::array({"(", ","})}}}, {"hoverProvider", true}, {"definitionProvider", true}, {"referencesProvider", true}, {"renameProvider", true}, {"documentSymbolProvider", true}, {"workspaceSymbolProvider", true}, {"semanticTokensProvider", {{"legend", {{"tokenTypes", json::array({"class", "type", "function", "method", "property", "variable", "parameter", "struct"})}, {"tokenModifiers", json::array()}}}, {"full", true}}}}}}}});
        else if (method == "luasharp/indexWorkspace") { indexedDocuments.clear(); for (const auto& document : (*message)["params"].value("documents", json::array())) { const auto uri = document.value("uri", ""); indexedDocuments[uri].SetText(document.value("text", "")); Place(uri, indexedDocuments[uri]); } ++exportsRevision; PublishOpenDiagnostics(); }
        // One file changed on disk. Standalone() advances exportsRevision only if its exported types differ, so other files stay cached otherwise.
        else if (method == "luasharp/indexDocument") { const auto& p = (*message)["params"]; const auto uri = p.value("uri", ""); indexedDocuments[uri].SetText(p.value("text", "")); Place(uri, indexedDocuments[uri]); PublishOpenDiagnostics(); }
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
            if (const auto* found = ExpressionSymbol(analysis, luasharp::FindExpression(analysis, offset))) symbol = found;
            if (!symbol) { const auto word = WordAt(text, p["position"]); const auto found = std::find_if(analysis.symbols.begin(), analysis.symbols.end(), [&](const auto& candidate) { return !candidate.implicit && candidate.name == word; }); if (found != analysis.symbols.end()) symbol = &*found; }
            if (method == "textDocument/definition") { json locations = json::array(); if (symbol && symbol->span.length) locations.push_back(Location(uri, text, *symbol)); else if (symbol) if (const auto external = ExternalLocation(documents[uri], *symbol)) locations.push_back(*external); Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", locations}}); }
            else Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", symbol ? json{{"contents", {{"kind", "markdown"}, {"value", "```luasharp\n" + HoverLabel(*symbol) + "\n```"}}}} : json(nullptr)}});
        } else if (method == "shutdown") Send({{"jsonrpc", "2.0"}, {"id", (*message)["id"]}, {"result", nullptr}});
        else if (method == "exit") break;
    }
}
