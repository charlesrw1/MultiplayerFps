#include "Compiler.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace luasharp {
namespace {

const std::unordered_set<std::string> Keywords{
    "class", "public", "private", "auto", "new", "if", "else", "return",
    "null", "true", "false"
};

TextSpan Cover(TextSpan first, TextSpan last) {
    return {first.offset, last.End() >= first.offset ? last.End() - first.offset : 0};
}

class Lexer {
public:
    explicit Lexer(std::string_view source) : source_(source) {}

    SyntaxTree Run() {
        SyntaxTree tree;
        while (position_ < source_.size()) {
            SkipTrivia();
            if (position_ >= source_.size()) break;
            const size_t start = position_;
            const unsigned char ch = static_cast<unsigned char>(source_[position_]);
            if (std::isalpha(ch) || ch == '_') LexIdentifier(tree);
            else if (std::isdigit(ch)) LexNumber(tree);
            else if (ch == '\'' || ch == '\"') LexString(tree);
            else LexSymbol(tree, start);
        }
        tree.tokens.push_back({SyntaxTokenKind::EndOfFile, {}, {source_.size(), 0}});
        tree.diagnostics.insert(tree.diagnostics.end(), pending_.begin(), pending_.end());
        return tree;
    }

private:
    void SkipTrivia() {
        for (;;) {
            while (position_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[position_]))) ++position_;
            if (source_.substr(position_, 2) == "//") {
                position_ += 2;
                while (position_ < source_.size() && source_[position_] != '\n') ++position_;
            } else if (source_.substr(position_, 2) == "/*") {
                const size_t start = position_;
                position_ += 2;
                const auto close = source_.find("*/", position_);
                if (close == std::string_view::npos) { pending_.push_back({"Unterminated block comment", {start, source_.size() - start}}); position_ = source_.size(); }
                else position_ = close + 2;
            } else break;
        }
    }
    void LexIdentifier(SyntaxTree& tree) {
        const size_t start = position_++;
        while (position_ < source_.size()) {
            const unsigned char ch = static_cast<unsigned char>(source_[position_]);
            if (!std::isalnum(ch) && ch != '_') break;
            ++position_;
        }
        std::string text(source_.substr(start, position_ - start));
        tree.tokens.push_back({Keywords.contains(text) ? SyntaxTokenKind::Keyword : SyntaxTokenKind::Identifier, std::move(text), {start, position_ - start}});
    }
    void LexNumber(SyntaxTree& tree) {
        const size_t start = position_++;
        bool dot = false;
        while (position_ < source_.size()) {
            const char ch = source_[position_];
            if (std::isdigit(static_cast<unsigned char>(ch))) { ++position_; continue; }
            if (ch == '.' && !dot && position_ + 1 < source_.size() && std::isdigit(static_cast<unsigned char>(source_[position_ + 1]))) { dot = true; ++position_; continue; }
            break;
        }
        tree.tokens.push_back({SyntaxTokenKind::Number, std::string(source_.substr(start, position_ - start)), {start, position_ - start}});
    }
    void LexString(SyntaxTree& tree) {
        const size_t start = position_; const char quote = source_[position_++]; bool closed = false;
        while (position_ < source_.size()) {
            if (source_[position_] == '\\' && position_ + 1 < source_.size()) { position_ += 2; continue; }
            if (source_[position_++] == quote) { closed = true; break; }
        }
        tree.tokens.push_back({SyntaxTokenKind::String, std::string(source_.substr(start, position_ - start)), {start, position_ - start}});
        if (!closed) tree.diagnostics.push_back({"Unterminated string literal", {start, position_ - start}});
    }
    void LexSymbol(SyntaxTree& tree, size_t start) {
        static constexpr std::string_view symbols = "{}()[];:,.+-*/%=!<>";
        static constexpr std::string_view pairs[] = {"!=", "==", "<=", ">=", "&&", "||", "+=", "-=", "*=", "/="};
        for (const auto pair : pairs) {
            if (source_.substr(position_, 2) == pair) { position_ += 2; tree.tokens.push_back({SyntaxTokenKind::Symbol, std::string(pair), {start, 2}}); return; }
        }
        const char ch = source_[position_++];
        if (symbols.find(ch) != std::string_view::npos) tree.tokens.push_back({SyntaxTokenKind::Symbol, std::string(1, ch), {start, 1}});
        else {
            tree.tokens.push_back({SyntaxTokenKind::Bad, std::string(1, ch), {start, 1}});
            tree.diagnostics.push_back({std::string("Unexpected character '") + ch + "'", {start, 1}});
        }
    }

    std::string_view source_;
    size_t position_{};
    std::vector<SyntaxDiagnostic> pending_;
};

class Parser {
public:
    Parser(std::vector<SyntaxToken> tokens, std::vector<SyntaxDiagnostic> diagnostics, size_t sourceLength)
        : tokens_(std::move(tokens)), diagnostics_(std::move(diagnostics)), sourceLength_(sourceLength) {}

    SyntaxTree Run() {
        SyntaxProgram program; program.span = {0, sourceLength_};
        while (!AtEnd()) {
            const size_t before = index_;
            if (Is("class")) program.classes.push_back(ParseClass());
            else program.topLevel.push_back(ParseStatement());
            if (index_ == before) Advance();
        }
        return {std::move(program), std::move(tokens_), std::move(diagnostics_)};
    }

    SyntaxExpression RunExpression() {
        auto expression = ParseExpression();
        if (!AtEnd()) diagnostics_.push_back({"Unexpected token '" + Current().text + "' in expression", Current().span});
        return expression;
    }
    const std::vector<SyntaxDiagnostic>& Diagnostics() const { return diagnostics_; }

private:
    const SyntaxToken& Current(size_t lookahead = 0) const { return tokens_[(std::min)(index_ + lookahead, tokens_.size() - 1)]; }
    bool AtEnd() const { return Current().kind == SyntaxTokenKind::EndOfFile; }
    bool Is(std::string_view text, size_t lookahead = 0) const { return Current(lookahead).text == text; }
    SyntaxToken Advance() { const auto token = Current(); if (!AtEnd()) ++index_; return token; }
    SyntaxToken Match(std::string_view text) {
        if (Is(text)) return Advance();
        diagnostics_.push_back({"Expected '" + std::string(text) + "'", Current().span});
        return {SyntaxTokenKind::Bad, std::string(text), {Current().span.offset, 0}};
    }
    SyntaxToken Identifier(std::string_view purpose = "identifier") {
        if (Current().kind == SyntaxTokenKind::Identifier) return Advance();
        diagnostics_.push_back({"Expected " + std::string(purpose), Current().span});
        if (!AtEnd() && Current().kind != SyntaxTokenKind::Symbol) return Advance();
        return {SyntaxTokenKind::Bad, "<missing>", {Current().span.offset, 0}};
    }
    std::pair<std::string, TextSpan> ParseType() {
        const auto first = Identifier("type name"); std::string type = first.text; TextSpan span = first.span;
        while (Is("[")) { const auto open = Advance(); const auto close = Match("]"); type += "[]"; span = Cover(span, close.span.length ? close.span : open.span); }
        return {std::move(type), span};
    }
    bool LooksLikeLocal() const {
        if (Current().kind != SyntaxTokenKind::Identifier) return false;
        size_t at = 1;
        while (Is("[", at) && Is("]", at + 1)) at += 2;
        return Current(at).kind == SyntaxTokenKind::Identifier && (Is("=", at + 1) || Is(";", at + 1));
    }
    std::vector<SyntaxStatement> ParseBlock() {
        Match("{"); std::vector<SyntaxStatement> body;
        while (!AtEnd() && !Is("}")) {
            const size_t before = index_; body.push_back(ParseStatement());
            if (before == index_) Advance();
        }
        Match("}"); return body;
    }
    SyntaxStatement ParseStatement() {
        const auto start = Current().span;
        if (Is("if")) {
            Advance(); Match("("); auto condition = ParseExpression(); Match(")");
            SyntaxStatement result; result.kind = SyntaxStatement::Kind::If; result.expression = std::move(condition); result.body = ParseBlock();
            if (Is("else")) { Advance(); result.elseBody = Is("if") ? std::vector<SyntaxStatement>{ParseStatement()} : ParseBlock(); }
            result.span = Cover(start, PreviousSpan()); return result;
        }
        if (Is("return")) {
            Advance(); SyntaxStatement result; result.kind = SyntaxStatement::Kind::Return;
            if (!Is(";")) result.expression = ParseExpression(); Match(";"); result.span = Cover(start, PreviousSpan()); return result;
        }
        if (Is("auto") || LooksLikeLocal()) {
            SyntaxStatement result; result.kind = SyntaxStatement::Kind::Local;
            if (Is("auto")) { Advance(); result.inferredType = true; }
            else result.type = ParseType().first;
            const auto name = Identifier("variable name"); result.name = name.text; result.nameSpan = name.span;
            if (Is("=")) { Advance(); result.expression = ParseExpression(); }
            else if (result.inferredType) diagnostics_.push_back({"An auto variable requires an initializer", name.span});
            Match(";"); result.span = Cover(start, PreviousSpan()); return result;
        }
        SyntaxStatement result; result.kind = SyntaxStatement::Kind::Expression; result.expression = ParseExpression();
        Match(";"); result.span = Cover(start, PreviousSpan()); return result;
    }
    SyntaxFunction ParseFunction(std::string access, SyntaxToken name, std::string returnType, bool constructor) {
        SyntaxFunction result; result.access = std::move(access); result.name = name.text; result.returnType = std::move(returnType); result.isConstructor = constructor; result.span = name.span;
        Match("(");
        while (!AtEnd() && !Is(")")) {
            auto [type, typeSpan] = ParseType(); const auto parameterName = Identifier("parameter name");
            result.parameters.push_back({std::move(type), parameterName.text, Cover(typeSpan, parameterName.span)});
            if (!Is(",")) break; Advance();
        }
        Match(")"); result.body = ParseBlock(); return result;
    }
    SyntaxClass ParseClass() {
        Match("class"); const auto name = Identifier("class name"); SyntaxClass result; result.name = name.text; result.span = name.span;
        if (Is(":")) { Advance(); result.baseName = Identifier("base type").text; }
        Match("{"); std::string access = "public";
        while (!AtEnd() && !Is("}")) {
            if ((Is("public") || Is("private")) && Is(":", 1)) { access = Advance().text; Advance(); continue; }
            const size_t before = index_;
            const auto first = Identifier();
            if (Is("(")) result.functions.push_back(ParseFunction(access, first, {}, true));
            else {
                std::string type = first.text;
                while (Is("[")) { Advance(); Match("]"); type += "[]"; }
                const auto memberName = Identifier("member name");
                if (Is("(")) result.functions.push_back(ParseFunction(access, memberName, std::move(type), false));
                else {
                    SyntaxField field; field.access = access; field.type = std::move(type); field.name = memberName.text; field.span = memberName.span;
                    if (Is("=")) { Advance(); field.initializer = ParseExpression(); }
                    Match(";"); result.fields.push_back(std::move(field));
                }
            }
            if (before == index_) Advance();
        }
        Match("}"); return result;
    }

    static int Precedence(std::string_view op) {
        if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=") return 1;
        if (op == "||") return 2; if (op == "&&") return 3;
        if (op == "==" || op == "!=") return 4;
        if (op == "<" || op == ">" || op == "<=" || op == ">=") return 5;
        if (op == "+" || op == "-") return 6; if (op == "*" || op == "/" || op == "%") return 7;
        return -1;
    }
    SyntaxExpression ParseExpression(int minimum = 0) {
        auto left = ParseUnary();
        while (!AtEnd()) {
            const auto operation = Current(); const int precedence = Precedence(operation.text);
            if (precedence < minimum) break;
            Advance(); const bool assignment = precedence == 1;
            auto right = ParseExpression(precedence + (assignment ? 0 : 1));
            left = {assignment ? SyntaxExpression::Kind::Assignment : SyntaxExpression::Kind::Binary, operation.text, Cover(left.span, right.span), {std::move(left), std::move(right)}};
        }
        return left;
    }
    SyntaxExpression ParseUnary() {
        if (Is("!") || Is("-") || Is("+")) { const auto op = Advance(); auto operand = ParseUnary(); return {SyntaxExpression::Kind::Unary, op.text, Cover(op.span, operand.span), {std::move(operand)}}; }
        return ParsePostfix(ParsePrimary());
    }
    SyntaxExpression ParsePrimary() {
        const auto token = Current();
        if (AtEnd() || Is(";") || Is(")") || Is("]") || Is("}" ) || Is(",")) {
            diagnostics_.push_back({"Expected expression", token.span}); return {SyntaxExpression::Kind::Missing, {}, {token.span.offset, 0}, {}};
        }
        Advance();
        if (token.text == "new") {
            const auto type = Identifier("type name after 'new'"); SyntaxExpression result{SyntaxExpression::Kind::New, type.text, Cover(token.span, type.span), {}};
            if (Is("(")) { auto arguments = ParseArguments(); if (!arguments.empty()) result.span = Cover(token.span, arguments.back().span); result.children = std::move(arguments); }
            else diagnostics_.push_back({"Expected '(' after constructed type", Current().span});
            return result;
        }
        if (token.text == "(") { auto value = ParseExpression(); const auto close = Match(")"); return {SyntaxExpression::Kind::Parenthesized, {}, Cover(token.span, close.span), {std::move(value)}}; }
        if (token.text == "{") { const auto close = Match("}"); return {SyntaxExpression::Kind::Table, "{}", Cover(token.span, close.span), {}}; }
        if (token.text == "null") return {SyntaxExpression::Kind::Null, token.text, token.span, {}};
        if (token.text == "true" || token.text == "false") return {SyntaxExpression::Kind::Boolean, token.text, token.span, {}};
        if (token.kind == SyntaxTokenKind::Number) return {SyntaxExpression::Kind::Number, token.text, token.span, {}};
        if (token.kind == SyntaxTokenKind::String) return {SyntaxExpression::Kind::String, token.text, token.span, {}};
        if (token.kind != SyntaxTokenKind::Identifier) diagnostics_.push_back({"Expected expression", token.span});
        return {token.kind == SyntaxTokenKind::Identifier ? SyntaxExpression::Kind::Identifier : SyntaxExpression::Kind::Missing, token.text, token.span, {}};
    }
    std::vector<SyntaxExpression> ParseArguments() {
        Match("("); std::vector<SyntaxExpression> arguments;
        while (!AtEnd() && !Is(")")) {
            arguments.push_back(ParseExpression());
            if (!Is(",")) break; Advance();
        }
        Match(")"); return arguments;
    }
    SyntaxExpression ParsePostfix(SyntaxExpression value) {
        while (!AtEnd()) {
            if (Is(".")) {
                Advance(); const auto name = Identifier("member name"); value = {SyntaxExpression::Kind::Member, name.text, Cover(value.span, name.span), {std::move(value)}};
            } else if (Is("(")) {
                const auto start = value.span; auto arguments = ParseArguments(); std::vector<SyntaxExpression> children; children.push_back(std::move(value));
                for (auto& argument : arguments) children.push_back(std::move(argument));
                value = {SyntaxExpression::Kind::Call, {}, arguments.empty() ? start : Cover(start, arguments.back().span), std::move(children)};
            } else if (Is("[")) {
                Advance(); const auto start = value.span; auto first = ParseExpression();
                if (Is(":")) { Advance(); auto last = ParseExpression(); const auto close = Match("]"); value = {SyntaxExpression::Kind::Slice, {}, Cover(start, close.span), {std::move(value), std::move(first), std::move(last)}}; }
                else { const auto close = Match("]"); value = {SyntaxExpression::Kind::Index, {}, Cover(start, close.span), {std::move(value), std::move(first)}}; }
            } else break;
        }
        return value;
    }
    TextSpan PreviousSpan() const { return index_ == 0 ? TextSpan{} : tokens_[index_ - 1].span; }

    std::vector<SyntaxToken> tokens_;
    std::vector<SyntaxDiagnostic> diagnostics_;
    size_t sourceLength_{};
    size_t index_{};
};

std::string LuaType(const std::string& type) {
    if (type == "int") return "integer"; if (type == "float") return "number"; if (type == "bool") return "boolean"; return type;
}

std::string EmitExpression(const SyntaxExpression& value) {
    const auto child = [&](size_t index) { return EmitExpression(value.children.at(index)); };
    switch (value.kind) {
    case SyntaxExpression::Kind::Missing: return {};
    case SyntaxExpression::Kind::Identifier: return value.text == "this" ? "self" : value.text;
    case SyntaxExpression::Kind::Number: case SyntaxExpression::Kind::String:
    case SyntaxExpression::Kind::Boolean: case SyntaxExpression::Kind::Table: return value.text;
    case SyntaxExpression::Kind::Null: return "nil";
    case SyntaxExpression::Kind::Parenthesized: return "(" + child(0) + ")";
    case SyntaxExpression::Kind::New: { std::string out = value.text + ".new("; for (size_t i{}; i < value.children.size(); ++i) { if (i) out += ", "; out += child(i); } return out + ")"; }
    case SyntaxExpression::Kind::Member: return child(0) + "." + value.text;
    case SyntaxExpression::Kind::Call: {
        const auto& callee = value.children.at(0); std::string out;
        if (callee.kind == SyntaxExpression::Kind::Member) out = EmitExpression(callee.children.at(0)) + ":" + callee.text;
        else out = EmitExpression(callee);
        out += "("; for (size_t i = 1; i < value.children.size(); ++i) { if (i != 1) out += ", "; out += child(i); } return out + ")";
    }
    case SyntaxExpression::Kind::Index: return child(0) + "[" + child(1) + "]";
    case SyntaxExpression::Kind::Slice: return "LuaSharp.slice(" + child(0) + ", " + child(1) + ", " + child(2) + ")";
    case SyntaxExpression::Kind::Unary: return (value.text == "!" ? "not " : value.text) + child(0);
    case SyntaxExpression::Kind::Assignment: {
        std::string op = value.text; if (op.size() == 2 && op[1] == '=') return child(0) + " = " + child(0) + " " + op.substr(0, 1) + " " + child(1);
        return child(0) + " = " + child(1);
    }
    case SyntaxExpression::Kind::Binary: { const auto op = value.text == "!=" ? "~=" : value.text == "&&" ? "and" : value.text == "||" ? "or" : value.text; return child(0) + " " + op + " " + child(1); }
    }
    return {};
}

struct Emitter {
    std::string output;
    std::vector<SourceMapEntry> sourceMap;
    void Raw(std::string text) { output += std::move(text); }
    void Line(std::string text, size_t sourceOffset) {
        sourceMap.push_back({1 + static_cast<size_t>(std::count(output.begin(), output.end(), '\n')), sourceOffset}); output += text + "\n";
    }
};

void EmitStatements(Emitter& emitter, const std::vector<SyntaxStatement>& statements, const std::string& indent) {
    for (const auto& statement : statements) {
        switch (statement.kind) {
        case SyntaxStatement::Kind::If:
            emitter.Line(indent + "if " + EmitExpression(statement.expression) + " then", statement.span.offset);
            EmitStatements(emitter, statement.body, indent + "    ");
            if (!statement.elseBody.empty()) { emitter.Line(indent + "else", statement.span.offset); EmitStatements(emitter, statement.elseBody, indent + "    "); }
            emitter.Line(indent + "end", statement.span.offset); break;
        case SyntaxStatement::Kind::Return: emitter.Line(indent + "return" + (statement.expression.kind == SyntaxExpression::Kind::Missing ? "" : " " + EmitExpression(statement.expression)), statement.span.offset); break;
        case SyntaxStatement::Kind::Local: emitter.Line(indent + "local " + statement.name + (statement.expression.kind == SyntaxExpression::Kind::Missing ? "" : " = " + EmitExpression(statement.expression)), statement.span.offset); break;
        case SyntaxStatement::Kind::Expression: emitter.Line(indent + EmitExpression(statement.expression), statement.span.offset); break;
        }
    }
}

void EmitClass(Emitter& emitter, const SyntaxClass& value) {
    emitter.Raw("---@class " + value.name + (value.baseName.empty() ? "" : " : " + value.baseName) + "\n");
    for (const auto& field : value.fields) {
        emitter.Raw("---@field " + field.access + " " + field.name + " " + LuaType(field.type));
        if (field.initializer.kind == SyntaxExpression::Kind::Null) emitter.Raw("?"); emitter.Raw("\n");
    }
    emitter.Raw("local " + value.name + " = {}\n" + value.name + ".__index = " + value.name + "\n");
    if (!value.baseName.empty()) emitter.Raw("setmetatable(" + value.name + ", { __index = " + value.baseName + " })\n");
    emitter.Raw("\n");
    const auto constructor = std::find_if(value.functions.begin(), value.functions.end(), [](const auto& f) { return f.isConstructor; });
    emitter.Raw("function " + value.name + ".new(");
    if (constructor != value.functions.end()) for (size_t i{}; i < constructor->parameters.size(); ++i) { if (i) emitter.Raw(", "); emitter.Raw(constructor->parameters[i].name); }
    emitter.Raw(")\n    local self = setmetatable({}, " + value.name + ")\n");
    for (const auto& field : value.fields) if (field.initializer.kind != SyntaxExpression::Kind::Missing) emitter.Line("    self." + field.name + " = " + EmitExpression(field.initializer), field.span.offset);
    if (constructor != value.functions.end()) EmitStatements(emitter, constructor->body, "    ");
    emitter.Raw("    return self\nend\n\n");
    for (const auto& function : value.functions) {
        if (function.isConstructor) continue;
        for (const auto& parameter : function.parameters) emitter.Raw("---@param " + parameter.name + " " + LuaType(parameter.type) + "\n");
        if (!function.returnType.empty() && function.returnType != "void") emitter.Raw("---@return " + LuaType(function.returnType) + "\n");
        emitter.Raw("function " + value.name + ":" + function.name + "(");
        for (size_t i{}; i < function.parameters.size(); ++i) { if (i) emitter.Raw(", "); emitter.Raw(function.parameters[i].name); }
        emitter.Raw(")\n"); EmitStatements(emitter, function.body, "    "); emitter.Raw("end\n\n");
    }
}

std::runtime_error SyntaxError(const SyntaxDiagnostic& diagnostic) {
    return std::runtime_error(diagnostic.message + " at " + std::to_string(diagnostic.span.offset));
}

} // namespace

SyntaxTree ParseTree(std::string_view source) {
    auto lexed = Lexer(source).Run();
    return Parser(std::move(lexed.tokens), std::move(lexed.diagnostics), source.size()).Run();
}

SyntaxProgram ParseSyntax(std::string_view source) {
    auto tree = ParseTree(source); if (!tree.diagnostics.empty()) throw SyntaxError(tree.diagnostics.front()); return std::move(tree.root);
}

SyntaxExpression ParseExpression(const std::vector<SyntaxToken>& tokens) {
    std::vector<SyntaxToken> copy = tokens;
    const size_t end = copy.empty() ? 0 : copy.back().span.End();
    copy.push_back({SyntaxTokenKind::EndOfFile, {}, {end, 0}});
    Parser parser(std::move(copy), {}, end); auto expression = parser.RunExpression();
    if (!parser.Diagnostics().empty()) throw SyntaxError(parser.Diagnostics().front());
    return expression;
}

std::string Compile(std::string_view source) { return CompileWithSourceMap(source).lua; }

CompilationResult CompileWithSourceMap(std::string_view source) {
    auto tree = ParseTree(source); if (!tree.diagnostics.empty()) throw SyntaxError(tree.diagnostics.front());
    Emitter emitter; for (const auto& value : tree.root.classes) EmitClass(emitter, value); EmitStatements(emitter, tree.root.topLevel, {});
    return {std::move(emitter.output), std::move(emitter.sourceMap)};
}

} // namespace luasharp
