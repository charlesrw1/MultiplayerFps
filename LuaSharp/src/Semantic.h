#pragma once

#include "Compiler.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace luasharp {

enum class SymbolKind { Class, Type, Field, Method, Variable, Parameter };
struct Symbol {
    std::string name;
    std::string type;
    SymbolKind kind{SymbolKind::Variable};
    size_t line{};
    size_t character{};
    TextSpan span;
    std::vector<std::string> parameters;
};
enum class DiagnosticSeverity { Error, Warning };
struct Diagnostic {
    std::string message;
    size_t line{};
    size_t character{};
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    TextSpan span;
};
struct ExpressionInfo {
    TextSpan span;
    std::string type;
    std::optional<size_t> symbolIndex;
};
struct VariableBinding {
    std::string name;
    std::string type;
    TextSpan declaration;
    TextSpan visibility;
    size_t depth{};
    size_t symbolIndex{};
};
struct Analysis {
    SyntaxTree syntax;
    std::vector<Symbol> symbols;
    std::vector<Diagnostic> diagnostics;
    std::vector<ExpressionInfo> expressions;
    std::vector<VariableBinding> bindings;
    std::unordered_map<std::string, std::vector<Symbol>> members;
    // Compatibility summary for existing consumers. Scoped queries should use
    // ResolveVariableType instead.
    std::unordered_map<std::string, std::string> variableTypes;
    std::unordered_map<std::string, std::string> baseTypes;
};

Analysis Analyze(std::string_view source);
std::optional<std::string> ResolveVariableType(const Analysis& analysis, std::string_view name, size_t position);
const ExpressionInfo* FindExpression(const Analysis& analysis, size_t position);

} // namespace luasharp
