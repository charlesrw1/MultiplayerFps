#include "Compiler.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace luasharp {
namespace {

enum class TokenKind { Identifier, Number, String, Symbol, End };
struct Token { TokenKind kind; std::string text; size_t offset; };
struct Parameter { std::string type; std::string name; };
struct Field { std::string access; std::string type; std::string name; std::vector<Token> initializer; };
struct Statement {
    enum class Kind { If, Return, Local, Expression } kind;
    std::string name;
    std::vector<Token> expression;
    std::vector<Statement> body;
    std::vector<Statement> elseBody;
    size_t sourceOffset{};
};
struct Function { std::string name; std::string returnType; bool isConstructor{}; std::vector<Parameter> parameters; std::vector<Statement> body; };
struct Class { std::string name; std::vector<Field> fields; std::vector<Function> functions; };
struct Program { std::vector<Class> classes; std::vector<Statement> topLevel; };

std::vector<Token> Lex(std::string_view source) {
    std::vector<Token> result;
    for (size_t i = 0; i < source.size();) {
        const auto start = i;
        const char ch = source[i];
        if (std::isspace(static_cast<unsigned char>(ch))) { ++i; continue; }
        if (source.substr(i, 2) == "//") { while (i < source.size() && source[i] != '\n') ++i; continue; }
        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
            while (i < source.size() && (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) ++i;
            result.push_back({TokenKind::Identifier, std::string(source.substr(start, i - start)), start}); continue;
        }
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            while (i < source.size() && (std::isdigit(static_cast<unsigned char>(source[i])) || source[i] == '.')) ++i;
            result.push_back({TokenKind::Number, std::string(source.substr(start, i - start)), start}); continue;
        }
        if (ch == '\'' || ch == '"') {
            const char quote = source[i++];
            while (i < source.size() && source[i] != quote) { if (source[i] == '\\') ++i; ++i; }
            if (i == source.size()) throw std::runtime_error("Unterminated string at " + std::to_string(start));
            result.push_back({TokenKind::String, std::string(source.substr(start, ++i - start)), start}); continue;
        }
        for (const std::string_view pair : {"!=", "==", "<=", ">=", "&&", "||", "+=", "-="}) {
            if (source.substr(i, pair.size()) == pair) { result.push_back({TokenKind::Symbol, std::string(pair), i}); i += pair.size(); goto next; }
        }
        if (std::string_view("{}()[];:,.+-*/=!<>").find(ch) != std::string_view::npos) { result.push_back({TokenKind::Symbol, std::string(1, ch), i++}); continue; }
        throw std::runtime_error(std::string("Unexpected character '") + ch + "' at " + std::to_string(i));
next:;
    }
    result.push_back({TokenKind::End, {}, source.size()});
    return result;
}

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    Program Parse() {
        Program program;
        while (Current().kind != TokenKind::End) {
            if (Is("class")) program.classes.push_back(ParseClass());
            else program.topLevel.push_back(ParseStatement());
        }
        return program;
    }

private:
    const Token& Current() const { return tokens_[index_]; }
    bool Is(std::string_view value) const { return Current().text == value; }
    Token Take(std::string_view expected = {}) {
        const Token token = Current();
        if (!expected.empty() && token.text != expected) throw std::runtime_error("Expected '" + std::string(expected) + "' at " + std::to_string(token.offset));
        ++index_;
        return token;
    }
    std::string Identifier() {
        const auto token = Take();
        if (token.kind != TokenKind::Identifier) throw std::runtime_error("Expected identifier at " + std::to_string(token.offset));
        return token.text;
    }
    std::string Type(std::string type = {}) {
        if (type.empty()) type = Identifier();
        while (Is("[")) { Take("["); Take("]"); type += "[]"; }
        return type;
    }
    std::vector<Token> Until(std::string_view end) {
        std::vector<Token> tokens;
        int depth = 0;
        while (!(Is(end) && depth == 0)) {
            if (Current().kind == TokenKind::End) throw std::runtime_error("Expected '" + std::string(end) + "'");
            auto token = Take();
            if (token.text == "(" || token.text == "[" || token.text == "{") ++depth;
            if (token.text == ")" || token.text == "]" || token.text == "}") --depth;
            tokens.push_back(std::move(token));
        }
        return tokens;
    }
    std::vector<Statement> Block() {
        Take("{"); std::vector<Statement> body;
        while (!Is("}")) body.push_back(ParseStatement());
        Take("}");
        return body;
    }
    Statement ParseStatement() {
        const size_t sourceOffset = Current().offset;
        if (Is("if")) {
            Take("if"); Take("("); Statement statement{Statement::Kind::If}; statement.expression = Until(")"); Take(")"); statement.body = Block();
            if (Is("else")) { Take("else"); statement.elseBody = Is("if") ? std::vector<Statement>{ParseStatement()} : Block(); }
            statement.sourceOffset = sourceOffset;
            return statement;
        }
        if (Is("return")) { Take("return"); Statement statement{Statement::Kind::Return}; statement.expression = Until(";"); Take(";"); statement.sourceOffset = sourceOffset; return statement; }
        if (Is("auto")) {
            Take("auto"); Statement statement{Statement::Kind::Local}; statement.name = Identifier();
            if (Is("=")) { Take("="); statement.expression = Until(";"); }
            Take(";"); statement.sourceOffset = sourceOffset; return statement;
        }
        Statement statement{Statement::Kind::Expression}; statement.expression = Until(";"); Take(";"); statement.sourceOffset = sourceOffset; return statement;
    }
    Function ParseFunction(std::string name, std::string returnType, bool isConstructor) {
        Function function{std::move(name), std::move(returnType), isConstructor}; Take("(");
        while (!Is(")")) { function.parameters.push_back({Type(), Identifier()}); if (!Is(",")) break; Take(","); }
        Take(")"); function.body = Block();
        return function;
    }
    Class ParseClass() {
        Take("class"); Class result; result.name = Identifier(); Take("{"); std::string access = "public";
        while (!Is("}")) {
            if ((Is("public") || Is("private")) && tokens_[index_ + 1].text == ":") { access = Take().text; Take(":"); continue; }
            const auto first = Identifier();
            if (Is("(")) { result.functions.push_back(ParseFunction(first, {}, true)); continue; }
            const auto type = Type(first); const auto name = Identifier();
            if (Is("(")) result.functions.push_back(ParseFunction(name, type, false));
            else {
                Field field{access, type, name};
                if (Is("=")) { Take("="); field.initializer = Until(";"); }
                Take(";"); result.fields.push_back(std::move(field));
            }
        }
        Take("}"); return result;
    }
    std::vector<Token> tokens_;
    size_t index_{};
};

std::string LuaType(const std::string& type) {
    if (type == "int") return "integer";
    if (type == "float") return "number";
    if (type == "bool") return "boolean";
    return type;
}

std::string Expression(const std::vector<Token>& tokens) {
    std::string out;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        const bool methodCall = token.text == "." && i + 2 < tokens.size() && tokens[i + 1].kind == TokenKind::Identifier && tokens[i + 2].text == "(";
        if (token.text == "new") {
            if (i + 1 >= tokens.size() || tokens[i + 1].kind != TokenKind::Identifier) throw std::runtime_error("Expected class name after 'new'");
            out += tokens[++i].text + ".new";
            continue;
        }
        if (token.text == "null") out += "nil";
        else if (token.text == "!=") out += "~=";
        else if (token.text == "&&") out += " and ";
        else if (token.text == "||") out += " or ";
        else if (methodCall) out += ":";
        else out += token.text;
        if (token.text == ",") out += ' ';
    }
    return out;
}

struct Emitter {
    std::string output;
    std::vector<SourceMapEntry> sourceMap;

    void Line(std::string text, size_t sourceOffset = 0) {
        sourceMap.push_back({1 + static_cast<size_t>(std::count(output.begin(), output.end(), '\n')), sourceOffset});
        output += text + "\n";
    }
};

void EmitStatements(Emitter& emitter, const std::vector<Statement>& statements, std::string indent) {
    for (const auto& statement : statements) {
        if (statement.kind == Statement::Kind::If) {
            emitter.Line(indent + "if " + Expression(statement.expression) + " then", statement.sourceOffset);
            EmitStatements(emitter, statement.body, indent + "    ");
            if (!statement.elseBody.empty()) { emitter.Line(indent + "else", statement.sourceOffset); EmitStatements(emitter, statement.elseBody, indent + "    "); }
            emitter.Line(indent + "end", statement.sourceOffset);
        } else if (statement.kind == Statement::Kind::Return) emitter.Line(indent + "return " + Expression(statement.expression), statement.sourceOffset);
        else if (statement.kind == Statement::Kind::Local) emitter.Line(indent + "local " + statement.name + (statement.expression.empty() ? "" : " = " + Expression(statement.expression)), statement.sourceOffset);
        else emitter.Line(indent + Expression(statement.expression), statement.sourceOffset);
    }
}

void EmitClass(Emitter& emitter, const Class& value) {
    auto& output = emitter.output;
    output += "---@class " + value.name + "\n";
    for (const auto& field : value.fields) {
        output += "---@field " + field.access + " " + field.name + " " + LuaType(field.type);
        if (!field.initializer.empty() && field.initializer.front().text == "null") output += "?";
        output += "\n";
    }
    output += "local " + value.name + " = {}\n" + value.name + ".__index = " + value.name + "\n\n";
    output += "function " + value.name + ".new()\n    local self = setmetatable({}, " + value.name + ")\n";
    for (const auto& field : value.fields) if (!field.initializer.empty()) output += "    self." + field.name + " = " + Expression(field.initializer) + "\n";
    if (const auto constructor = std::find_if(value.functions.begin(), value.functions.end(), [](const Function& f) { return f.isConstructor; }); constructor != value.functions.end()) EmitStatements(emitter, constructor->body, "    ");
    output += "    return self\nend\n\n";
    for (const auto& function : value.functions) {
        if (function.isConstructor) continue;
        for (const auto& parameter : function.parameters) output += "---@param " + parameter.name + " " + LuaType(parameter.type) + "\n";
        if (!function.returnType.empty() && function.returnType != "void") output += "---@return " + LuaType(function.returnType) + "\n";
        output += "function " + value.name + ":" + function.name + "(";
        for (size_t i = 0; i < function.parameters.size(); ++i) { if (i != 0) output += ", "; output += function.parameters[i].name; }
        output += ")\n"; EmitStatements(emitter, function.body, "    "); output += "end\n\n";
    }
}

} // namespace

std::string Compile(std::string_view source) {
    return CompileWithSourceMap(source).lua;
}

CompilationResult CompileWithSourceMap(std::string_view source) {
    const auto program = Parser(Lex(source)).Parse();
    Emitter emitter;
    for (const auto& value : program.classes) EmitClass(emitter, value);
    EmitStatements(emitter, program.topLevel, {});
    return {std::move(emitter.output), std::move(emitter.sourceMap)};
}

} // namespace luasharp
