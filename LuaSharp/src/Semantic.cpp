#include "Semantic.h"
#include "Compiler.h"

#include <algorithm>
#include <unordered_set>

namespace luasharp {
namespace {
struct Analyzer {
    std::string_view source;
    Analysis result;
    std::unordered_set<std::string> knownTypes{"int", "float", "bool", "string", "table", "void", "Entity", "Component"};
    std::vector<std::vector<SyntaxToken>> expressions;

    std::pair<size_t, size_t> Position(size_t offset) const {
        offset = (std::min)(offset, source.size()); size_t line{}, character{}, start{};
        while (start < offset) { const auto newline = source.find('\n', start); if (newline == std::string_view::npos || newline >= offset) break; ++line; start = newline + 1; }
        character = offset - start; return {line, character};
    }
    void AddSymbol(std::string name, std::string type, SymbolKind kind, size_t offset, std::vector<std::string> parameters = {}) {
        const auto [line, character] = Position(offset); result.symbols.push_back({std::move(name), std::move(type), kind, line, character, std::move(parameters)});
    }
    void Diagnostic(std::string message, size_t offset, DiagnosticSeverity severity = DiagnosticSeverity::Error) {
        const auto [line, character] = Position(offset); result.diagnostics.push_back({std::move(message), line, character, severity});
    }
    static std::string BaseType(std::string type) { while (type.ends_with("[]")) type.resize(type.size() - 2); return type; }
    static bool External(const std::string& type) { static const std::unordered_set<std::string> types{"int", "float", "bool", "string", "table", "Entity", "Component"}; return types.contains(BaseType(type)); }
    static bool IsNumber(const std::vector<SyntaxToken>& expression) { return expression.size() == 1 && !expression[0].text.empty() && std::all_of(expression[0].text.begin(), expression[0].text.end(), [](unsigned char c) { return (c >= '0' && c <= '9') || c == '.' || c == '-'; }); }
    bool Assignable(const std::string& expected, const std::vector<SyntaxToken>& expression) const {
        if (expression.empty() || expression[0].text == "null" || expression[0].text == "{" || expression[0].text == "new") return true;
        if (expected == "int") return IsNumber(expression) && expression[0].text.find('.') == std::string::npos;
        if (expected == "float") return IsNumber(expression);
        if (expected == "string") return expression[0].text.size() >= 2 && (expression[0].text.front() == '\"' || expression[0].text.front() == '\'');
        if (expected == "bool") return expression[0].text == "true" || expression[0].text == "false";
        return true;
    }
    void AddExpressionDiagnostics(const std::vector<SyntaxToken>& expression) {
        for (size_t i{}; i + 2 < expression.size(); ++i) {
            if (expression[i + 1].text != ".") continue;
            const auto variable = result.variableTypes.find(expression[i].text); if (variable == result.variableTypes.end() || External(variable->second)) continue;
            const auto members = result.members.find(BaseType(variable->second));
            const bool exists = members != result.members.end() && std::any_of(members->second.begin(), members->second.end(), [&](const Symbol& member) { return member.name == expression[i + 2].text; });
            if (!exists) Diagnostic("Unknown member '" + expression[i + 2].text + "' on " + BaseType(variable->second), expression[i + 2].offset, DiagnosticSeverity::Warning);
        }
    }
    void VisitStatement(const SyntaxStatement& statement) {
        if (statement.kind == SyntaxStatement::Kind::Local) {
            std::string type = statement.type;
            if (type.empty() && statement.expression.size() >= 2 && statement.expression[0].text == "new") type = statement.expression[1].text;
            if (!type.empty()) { AddSymbol(statement.name, type, SymbolKind::Variable, statement.offset); result.variableTypes[statement.name] = type; if (!Assignable(type, statement.expression)) Diagnostic("Cannot assign value to " + type, statement.expression.empty() ? statement.offset : statement.expression.front().offset); }
        }
        expressions.push_back(statement.expression);
        for (const auto& child : statement.body) VisitStatement(child);
        for (const auto& child : statement.elseBody) VisitStatement(child);
    }
    void VisitClass(const SyntaxClass& value) {
        knownTypes.insert(value.name); AddSymbol(value.name, value.name, SymbolKind::Class, value.offset);
        if (!value.baseName.empty()) result.baseTypes[value.name] = value.baseName;
        for (const auto& field : value.fields) { AddSymbol(field.name, field.type, SymbolKind::Field, field.offset); result.variableTypes[field.name] = field.type; result.members[value.name].push_back(result.symbols.back()); expressions.push_back(field.initializer); if (!Assignable(field.type, field.initializer)) Diagnostic("Cannot assign value to " + field.type, field.initializer.empty() ? field.offset : field.initializer.front().offset); }
        for (const auto& function : value.functions) {
            std::vector<std::string> parameters; for (const auto& parameter : function.parameters) parameters.push_back(parameter.type + " " + parameter.name);
            AddSymbol(function.name, function.returnType.empty() ? value.name : function.returnType, SymbolKind::Method, function.offset, parameters); result.members[value.name].push_back(result.symbols.back());
            for (const auto& parameter : function.parameters) { AddSymbol(parameter.name, parameter.type, SymbolKind::Parameter, parameter.offset); result.variableTypes[parameter.name] = parameter.type; }
            for (const auto& statement : function.body) VisitStatement(statement);
        }
    }
    void ValidateTypes() {
        for (const auto& symbol : result.symbols) {
            if (symbol.kind == SymbolKind::Class) continue; const auto type = BaseType(symbol.type);
            if (!type.empty() && !knownTypes.contains(type)) result.diagnostics.push_back({"Unknown type '" + type + "'", symbol.line, symbol.character});
        }
    }
    void FlattenInheritance() {
        for (const auto& [derived, base] : result.baseTypes) { std::string current = base; for (int depth = 0; depth != 32 && !current.empty(); ++depth) { const auto inherited = result.members.find(current); if (inherited != result.members.end()) result.members[derived].insert(result.members[derived].end(), inherited->second.begin(), inherited->second.end()); const auto parent = result.baseTypes.find(current); current = parent == result.baseTypes.end() ? "" : parent->second; } }
    }
};
}

Analysis Analyze(std::string_view source) {
    Analyzer analyzer{source}; SyntaxProgram program;
    try { program = ParseSyntax(source); } catch (...) { return analyzer.result; }
    for (const auto& value : program.classes) analyzer.knownTypes.insert(value.name);
    for (const auto& value : program.classes) analyzer.VisitClass(value);
    for (const auto& statement : program.topLevel) analyzer.VisitStatement(statement);
    analyzer.FlattenInheritance();
    for (const auto& expression : analyzer.expressions) analyzer.AddExpressionDiagnostics(expression);
    analyzer.ValidateTypes();
    return std::move(analyzer.result);
}
} // namespace luasharp
