#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace luasharp {

enum class SymbolKind { Class, Type, Field, Method, Variable, Parameter };
struct Symbol { std::string name; std::string type; SymbolKind kind; size_t line; size_t character; std::vector<std::string> parameters; };
enum class DiagnosticSeverity { Error, Warning };
struct Diagnostic { std::string message; size_t line; size_t character; DiagnosticSeverity severity{DiagnosticSeverity::Error}; };
struct Analysis {
    std::vector<Symbol> symbols;
    std::vector<Diagnostic> diagnostics;
    std::unordered_map<std::string, std::vector<Symbol>> members;
    std::unordered_map<std::string, std::string> variableTypes;
    std::unordered_map<std::string, std::string> baseTypes;
};

Analysis Analyze(std::string_view source);

} // namespace luasharp
