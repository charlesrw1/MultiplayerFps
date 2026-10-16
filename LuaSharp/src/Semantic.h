#pragma once

#include "Compiler.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace luasharp {

enum class SymbolKind { Class, Enum, Type, Field, Method, Function, Variable, Parameter };
struct Symbol {
    std::string name;
    std::string type;
    SymbolKind kind{SymbolKind::Variable};
    size_t line{};
    size_t character{};
    TextSpan span;
    std::vector<std::string> parameters;
    std::string owner;
    std::string access{"public"};
};
struct ExternalType {
    std::string name;
    std::string baseType;
    std::vector<Symbol> members;
    bool isEnum{};
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
    std::string lowering;
};
struct VariableBinding {
    std::string name;
    std::string type;
    TextSpan declaration;
    TextSpan visibility;
    size_t depth{};
    size_t symbolIndex{};
};
struct BoundStatement;
struct BoundExpression {
    SyntaxExpression::Kind kind{SyntaxExpression::Kind::Missing};
    std::string text;
    TextSpan span;
    std::string type;
    std::optional<size_t> symbolIndex;
    std::string lowering;
    std::vector<BoundExpression> children;
    std::vector<SyntaxParameter> parameters;
    std::vector<BoundStatement> lambdaBody;
    bool lambdaIsBlock{};
};
struct BoundStatement {
    SyntaxStatement::Kind kind{SyntaxStatement::Kind::Expression};
    std::string name;
    std::string secondName;
    std::string type;
    bool inferredType{};
    TextSpan span;
    BoundExpression expression;
    BoundExpression increment;
    std::vector<BoundStatement> initializer;
    std::vector<BoundStatement> body;
    std::vector<BoundStatement> elseBody;
};
struct BoundFunction {
    std::string access;
    std::string name;
    std::string returnType;
    bool isConstructor{};
    std::vector<SyntaxParameter> parameters;
    std::vector<BoundStatement> body;
    TextSpan span;
};
struct BoundField {
    std::string access;
    std::string type;
    std::string name;
    BoundExpression initializer;
    TextSpan span;
};
struct BoundClass {
    std::string name;
    std::string baseName;
    std::vector<BoundField> fields;
    std::vector<BoundFunction> functions;
    TextSpan span;
};
struct BoundEnumMember {
    std::string name;
    long long value{};
    TextSpan span;
};
struct BoundEnum {
    std::string name;
    std::vector<BoundEnumMember> members;
    TextSpan span;
};
struct BoundProgram {
    std::vector<BoundEnum> enums;
    std::vector<BoundClass> classes;
    std::vector<BoundStatement> topLevel;
    TextSpan span;
};
struct Analysis {
    SyntaxTree syntax;
    BoundProgram bound;
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
Analysis Analyze(std::string_view source, const std::vector<ExternalType>& externalTypes);
std::optional<std::string> ResolveVariableType(const Analysis& analysis, std::string_view name, size_t position);
const ExpressionInfo* FindExpression(const Analysis& analysis, size_t position);

} // namespace luasharp
