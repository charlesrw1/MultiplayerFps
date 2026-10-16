#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace luasharp {
struct SourceMapEntry {
    size_t generatedLine;
    size_t sourceOffset;
};

struct CompilationResult {
    std::string lua;
    std::vector<SourceMapEntry> sourceMap;
};

// Shared concrete syntax tree. The compiler and language server both consume
// this recursive-descent parse result; semantic analysis must never guess from
// individual source lines.
struct SyntaxToken { std::string text; size_t offset{}; };
struct SyntaxExpression {
    enum class Kind { Identifier, Number, String, Null, Boolean, Table, New, Member, Call, Index, Slice, Unary, Binary, Assignment } kind;
    std::string text;
    size_t offset{};
    std::vector<SyntaxExpression> children;
};
struct SyntaxParameter { std::string type; std::string name; size_t offset{}; };
struct SyntaxField { std::string access; std::string type; std::string name; std::vector<SyntaxToken> initializerTokens; SyntaxExpression initializer; size_t offset{}; };
struct SyntaxStatement {
    enum class Kind { If, Return, Local, Expression } kind;
    std::string name;
    std::string type;
    std::vector<SyntaxToken> expressionTokens;
    SyntaxExpression expression;
    std::vector<SyntaxStatement> body;
    std::vector<SyntaxStatement> elseBody;
    size_t offset{};
};
struct SyntaxFunction { std::string name; std::string returnType; bool isConstructor{}; std::vector<SyntaxParameter> parameters; std::vector<SyntaxStatement> body; size_t offset{}; };
struct SyntaxClass { std::string name; std::string baseName; std::vector<SyntaxField> fields; std::vector<SyntaxFunction> functions; size_t offset{}; };
struct SyntaxProgram { std::vector<SyntaxClass> classes; std::vector<SyntaxStatement> topLevel; };

std::string Compile(std::string_view source);
CompilationResult CompileWithSourceMap(std::string_view source);
SyntaxProgram ParseSyntax(std::string_view source);
SyntaxExpression ParseExpression(const std::vector<SyntaxToken>& tokens);
}
