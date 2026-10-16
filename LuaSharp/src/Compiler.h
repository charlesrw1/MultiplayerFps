#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace luasharp {

struct TextSpan {
    size_t offset{};
    size_t length{};
    [[nodiscard]] size_t End() const { return offset + length; }
    [[nodiscard]] bool Contains(size_t position) const { return position >= offset && position <= End(); }
};

enum class SyntaxTokenKind { Identifier, Number, String, Keyword, Symbol, EndOfFile, Bad };
struct SyntaxToken {
    SyntaxTokenKind kind{SyntaxTokenKind::Bad};
    std::string text;
    TextSpan span;
};

struct SyntaxDiagnostic {
    std::string message;
    TextSpan span;
};

struct SyntaxExpression {
    enum class Kind {
        Missing, Identifier, Number, String, Null, Boolean, Table, New,
        Member, Call, Index, Slice, Unary, Binary, Assignment, Parenthesized
    } kind{Kind::Missing};
    std::string text;
    TextSpan span;
    std::vector<SyntaxExpression> children;
};

struct SyntaxParameter { std::string type; std::string name; TextSpan span; };
struct SyntaxField {
    std::string access;
    std::string type;
    std::string name;
    SyntaxExpression initializer;
    TextSpan span;
};
struct SyntaxStatement {
    enum class Kind { If, Return, Local, Expression } kind{Kind::Expression};
    std::string name;
    std::string type;
    TextSpan nameSpan;
    bool inferredType{};
    SyntaxExpression expression;
    std::vector<SyntaxStatement> body;
    std::vector<SyntaxStatement> elseBody;
    TextSpan span;
};
struct SyntaxFunction {
    std::string access;
    std::string name;
    std::string returnType;
    bool isConstructor{};
    std::vector<SyntaxParameter> parameters;
    std::vector<SyntaxStatement> body;
    TextSpan span;
};
struct SyntaxClass {
    std::string name;
    std::string baseName;
    std::vector<SyntaxField> fields;
    std::vector<SyntaxFunction> functions;
    TextSpan span;
};
struct SyntaxProgram {
    std::vector<SyntaxClass> classes;
    std::vector<SyntaxStatement> topLevel;
    TextSpan span;
};
struct SyntaxTree {
    SyntaxProgram root;
    std::vector<SyntaxToken> tokens;
    std::vector<SyntaxDiagnostic> diagnostics;
};

struct SourceMapEntry {
    size_t generatedLine{};
    size_t sourceOffset{};
};
struct CompilationResult {
    std::string lua;
    std::vector<SourceMapEntry> sourceMap;
};

// ParseTree is error-tolerant and is the entry point for editor tooling.
SyntaxTree ParseTree(std::string_view source);
// Compatibility helpers for callers that require valid syntax.
SyntaxProgram ParseSyntax(std::string_view source);
SyntaxExpression ParseExpression(const std::vector<SyntaxToken>& tokens);

std::string Compile(std::string_view source);
CompilationResult CompileWithSourceMap(std::string_view source);

} // namespace luasharp
