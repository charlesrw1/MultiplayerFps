#include "Compiler.h"
#include "Builtins.h"
#include "Semantic.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace luasharp {
namespace {

const std::unordered_set<std::string> Keywords{
    "class", "enum", "public", "private", "auto", "new", "if", "else", "while", "for", "in", "break", "continue", "return",
    "null", "true", "false"
};

TextSpan Cover(TextSpan first, TextSpan last) {
    return {first.offset, last.End() >= first.offset ? last.End() - first.offset : 0};
}

class Lexer {
public:
    // Skips a UTF-8 byte order mark; spans still index the full source.
    explicit Lexer(std::string_view source) : source_(source), position_(source.starts_with("\xEF\xBB\xBF") ? 3 : 0) {}

    SyntaxTree Run() {
        SyntaxTree tree;
        tree.tokens.reserve(source_.size() / 3); // regrowing a large token vector costs more than lexing itself
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
        static constexpr std::string_view pairs[] = {"!=", "==", "<=", ">=", "&&", "||", "+=", "-=", "*=", "/=", "..", "=>"};
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

// Builds the syntax pools bottom-up: children are added before their parent.
// Child id lists collect on scratch_ and move to tree_.links once complete.
class Parser {
public:
    Parser(SyntaxTree lexed, size_t sourceLength) : tree_(std::move(lexed)), sourceLength_(sourceLength) {
        assert(tree_.expressions.empty() && !tree_.tokens.empty() && tree_.tokens.back().kind == SyntaxTokenKind::EndOfFile);
        // Estimates from token count; the pools still grow if a program exceeds them.
        tree_.expressions.reserve(tree_.tokens.size() + 1);
        tree_.links.reserve(tree_.tokens.size());
        tree_.statements.reserve(tree_.tokens.size() / 4 + 1);
        tree_.expressions.emplace_back(); // shared missing expression
    }

    SyntaxTree Run() {
        tree_.root.span = {0, sourceLength_};
        const auto mark = scratch_.size();
        while (!AtEnd()) {
            const size_t before = index_;
            if (Is("class")) { auto value = ParseClass(); tree_.root.classes.push_back(std::move(value)); }
            else if (Is("enum")) { auto value = ParseEnum(); tree_.root.enums.push_back(std::move(value)); }
            else { const auto statement = ParseStatement(); scratch_.push_back(statement); }
            if (index_ == before) Advance();
        }
        tree_.root.topLevel = Link(mark);
        return Take();
    }

    NodeId RunExpression() {
        const auto expression = ParseExpression();
        if (!AtEnd()) tree_.diagnostics.push_back({"Unexpected token '" + Current().text + "' in expression", Current().span});
        return expression;
    }
    const std::vector<SyntaxDiagnostic>& Diagnostics() const { return tree_.diagnostics; }
    SyntaxTree Take() { assert(scratch_.empty()); return std::move(tree_); }

private:
    const SyntaxToken& Current(size_t lookahead = 0) const { return tree_.tokens[(std::min)(index_ + lookahead, tree_.tokens.size() - 1)]; }
    bool AtEnd() const { return Current().kind == SyntaxTokenKind::EndOfFile; }
    bool Is(std::string_view text, size_t lookahead = 0) const { return Current(lookahead).text == text; }
    const SyntaxToken& Advance() { const auto& token = Current(); if (!AtEnd()) ++index_; return token; }
    SyntaxToken Match(std::string_view text) {
        if (Is(text)) return Advance();
        tree_.diagnostics.push_back({"Expected '" + std::string(text) + "'", Current().span});
        return {SyntaxTokenKind::Bad, std::string(text), {Current().span.offset, 0}};
    }
    SyntaxToken Identifier(std::string_view purpose = "identifier") {
        if (Current().kind == SyntaxTokenKind::Identifier) return Advance();
        tree_.diagnostics.push_back({"Expected " + std::string(purpose), Current().span});
        if (!AtEnd() && Current().kind != SyntaxTokenKind::Symbol) return Advance();
        return {SyntaxTokenKind::Bad, "<missing>", {Current().span.offset, 0}};
    }
    SyntaxToken MemberName() {
        if (Current().kind == SyntaxTokenKind::Identifier || Current().kind == SyntaxTokenKind::Keyword) return Advance();
        tree_.diagnostics.push_back({"Expected member name", Current().span});
        return {SyntaxTokenKind::Bad, "<missing>", {Current().span.offset, 0}};
    }

    NodeId Add(SyntaxExpression expression) {
        const auto id = static_cast<NodeId>(tree_.expressions.size());
        tree_.expressions.push_back(std::move(expression));
        return id;
    }
    NodeId Add(SyntaxExpression::Kind kind, std::string text, TextSpan span, NodeRange children = {}) {
        return Add(SyntaxExpression{kind, std::move(text), span, children});
    }
    NodeId Add(SyntaxStatement statement) {
        const auto id = static_cast<NodeId>(tree_.statements.size());
        tree_.statements.push_back(std::move(statement));
        return id;
    }
    // Moves the ids pushed on scratch_ since mark into links.
    NodeRange Link(size_t mark) {
        assert(mark <= scratch_.size());
        const auto first = static_cast<NodeId>(tree_.links.size());
        tree_.links.insert(tree_.links.end(), scratch_.begin() + static_cast<std::ptrdiff_t>(mark), scratch_.end());
        scratch_.resize(mark);
        return {first, static_cast<NodeId>(tree_.links.size() - first)};
    }
    NodeRange Link(std::initializer_list<NodeId> ids) {
        const auto first = static_cast<NodeId>(tree_.links.size());
        tree_.links.insert(tree_.links.end(), ids);
        return {first, static_cast<NodeId>(ids.size())};
    }
    TextSpan Span(NodeId expression) const { return tree_.Expression(expression).span; }

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
    NodeRange ParseBlock() {
        Match("{"); const auto mark = scratch_.size();
        while (!AtEnd() && !Is("}")) {
            const size_t before = index_; const auto statement = ParseStatement(); scratch_.push_back(statement);
            if (before == index_) Advance();
        }
        Match("}"); return Link(mark);
    }
    NodeId ParseStatement() {
        const auto start = Current().span;
        SyntaxStatement result;
        if (Is("if")) {
            Advance(); Match("("); result.expression = ParseExpression(); Match(")");
            result.kind = SyntaxStatement::Kind::If; result.body = ParseBlock();
            if (Is("else")) { Advance(); result.elseBody = Is("if") ? Link({ParseStatement()}) : ParseBlock(); }
        } else if (Is("while")) {
            Advance(); Match("("); result.expression = ParseExpression(); Match(")");
            result.kind = SyntaxStatement::Kind::While; result.body = ParseBlock();
        } else if (Is("for")) {
            Advance(); Match("(");
            const bool forEach = Current().kind == SyntaxTokenKind::Identifier && (Is("in", 1) || (Is(",", 1) && Current(2).kind == SyntaxTokenKind::Identifier && Is("in", 3)));
            if (forEach) {
                result.kind = SyntaxStatement::Kind::ForEach;
                const auto& firstName = Advance(); result.name = firstName.text; result.nameSpan = firstName.span;
                if (Is(",")) { Advance(); const auto secondName = Identifier("iteration variable"); result.secondName = secondName.text; result.secondNameSpan = secondName.span; }
                Match("in"); auto iterable = ParseExpression();
                if (Is("..")) { Advance(); const auto end = ParseExpression(); iterable = Add(SyntaxExpression::Kind::Range, "..", Cover(Span(iterable), Span(end)), Link({iterable, end})); }
                result.expression = iterable; Match(")"); result.body = ParseBlock();
            } else {
                result.kind = SyntaxStatement::Kind::For;
                if (!Is(";")) result.initializer = Link({ParseStatement()});
                else Advance();
                if (!Is(";")) result.expression = ParseExpression();
                else result.expression = Add(SyntaxExpression::Kind::Boolean, "true", Current().span);
                Match(";");
                if (!Is(")")) result.increment = ParseExpression();
                Match(")"); result.body = ParseBlock();
            }
        } else if (Is("break") || Is("continue")) {
            result.kind = Advance().text == "break" ? SyntaxStatement::Kind::Break : SyntaxStatement::Kind::Continue;
            Match(";");
        } else if (Is("return")) {
            Advance(); result.kind = SyntaxStatement::Kind::Return;
            if (!Is(";")) result.expression = ParseExpression();
            Match(";");
        } else if (Is("auto") || LooksLikeLocal()) {
            result.kind = SyntaxStatement::Kind::Local;
            if (Is("auto")) { Advance(); result.inferredType = true; }
            else result.type = ParseType().first;
            const auto name = Identifier("variable name"); result.name = name.text; result.nameSpan = name.span;
            if (Is("=")) { Advance(); result.expression = ParseExpression(); }
            else if (result.inferredType) tree_.diagnostics.push_back({"An auto variable requires an initializer", name.span});
            Match(";");
        } else {
            result.kind = SyntaxStatement::Kind::Expression; result.expression = ParseExpression();
            Match(";");
        }
        result.span = Cover(start, PreviousSpan());
        return Add(std::move(result));
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
    SyntaxEnum ParseEnum() {
        const auto start = Match("enum").span; const auto name = Identifier("enum name");
        SyntaxEnum result; result.name = name.text; result.nameSpan = name.span;
        Match("{");
        while (!AtEnd() && !Is("}")) {
            const auto memberName = Identifier("enum member"); SyntaxEnumMember member; member.name = memberName.text; member.nameSpan = memberName.span; member.span = memberName.span;
            if (Is("=")) { Advance(); member.initializer = ParseExpression(); member.span = Cover(memberName.span, Span(member.initializer)); }
            result.members.push_back(std::move(member));
            if (!Is(",") && !Is(";")) break; Advance();
        }
        const auto close = Match("}"); result.span = Cover(start, close.span); return result;
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
    bool LooksLikeLambda() const {
        if (Current().kind == SyntaxTokenKind::Identifier && Is("=>", 1)) return true;
        if (!Is("(")) return false;
        size_t at = 1;
        if (Is(")", at)) return Is("=>", at + 1);
        for (;;) {
            if (Current(at).kind != SyntaxTokenKind::Identifier) return false;
            ++at;
            if (Is(")", at)) return Is("=>", at + 1);
            if (!Is(",", at)) return false;
            ++at;
        }
    }
    NodeId ParseLambda() {
        const auto start = Current().span;
        const auto firstParameter = static_cast<NodeId>(tree_.parameters.size());
        if (Current().kind == SyntaxTokenKind::Identifier) {
            const auto& name = Advance(); tree_.parameters.push_back({"any", name.text, name.span});
        } else {
            Match("(");
            while (!AtEnd() && !Is(")")) {
                const auto name = Identifier("lambda parameter"); tree_.parameters.push_back({"any", name.text, name.span});
                if (!Is(",")) break; Advance();
            }
            Match(")");
        }
        SyntaxExpression result; result.kind = SyntaxExpression::Kind::Lambda; result.span = start;
        result.parameters = {firstParameter, static_cast<NodeId>(tree_.parameters.size() - firstParameter)};
        Match("=>");
        if (Is("{")) { result.lambdaIsBlock = true; result.lambdaBody = ParseBlock(); result.span = Cover(start, PreviousSpan()); }
        else { const auto body = ParseExpression(); result.span = Cover(start, Span(body)); result.children = Link({body}); }
        return Add(std::move(result));
    }
    NodeId ParseExpression(int minimum = 0) {
        auto left = ParseUnary();
        while (!AtEnd()) {
            const auto& operation = Current(); const int precedence = Precedence(operation.text);
            if (precedence < minimum) break;
            Advance(); const bool assignment = precedence == 1;
            const auto right = ParseExpression(precedence + (assignment ? 0 : 1));
            left = Add(assignment ? SyntaxExpression::Kind::Assignment : SyntaxExpression::Kind::Binary, operation.text, Cover(Span(left), Span(right)), Link({left, right}));
        }
        return left;
    }
    NodeId ParseUnary() {
        if (LooksLikeLambda()) return ParsePostfix(ParseLambda());
        if (Is("!") || Is("-") || Is("+")) {
            const auto& op = Advance(); const auto operand = ParseUnary();
            return Add(SyntaxExpression::Kind::Unary, op.text, Cover(op.span, Span(operand)), Link({operand}));
        }
        return ParsePostfix(ParsePrimary());
    }
    NodeId ParsePrimary() {
        const auto& token = Current();
        if (AtEnd() || Is(";") || Is(")") || Is("]") || Is("}" ) || Is(",")) {
            tree_.diagnostics.push_back({"Expected expression", token.span}); return Add(SyntaxExpression::Kind::Missing, {}, {token.span.offset, 0});
        }
        Advance();
        if (token.text == "new") {
            const auto type = Identifier("type name after 'new'"); SyntaxExpression result{SyntaxExpression::Kind::New, type.text, Cover(token.span, type.span)};
            if (Is("(")) {
                const auto mark = scratch_.size();
                if (PushArguments()) result.span = Cover(token.span, Span(scratch_.back()));
                result.children = Link(mark);
            }
            else tree_.diagnostics.push_back({"Expected '(' after constructed type", Current().span});
            return Add(std::move(result));
        }
        if (token.text == "(") { const auto value = ParseExpression(); const auto close = Match(")"); return Add(SyntaxExpression::Kind::Parenthesized, {}, Cover(token.span, close.span), Link({value})); }
        if (token.text == "{") {
            const auto mark = scratch_.size();
            while (!AtEnd() && !Is("}")) {
                NodeId entry{};
                if (Current().kind == SyntaxTokenKind::Identifier && Is("=", 1)) {
                    const auto& key = Advance(); Advance(); const auto value = ParseExpression();
                    entry = Add(SyntaxExpression::Kind::TableEntry, key.text, Cover(key.span, Span(value)), Link({value}));
                } else if (Is("[")) {
                    const auto& open = Advance(); const auto key = ParseExpression(); Match("]"); Match("="); const auto value = ParseExpression();
                    entry = Add(SyntaxExpression::Kind::TableEntry, {}, Cover(open.span, Span(value)), Link({key, value}));
                } else entry = ParseExpression();
                scratch_.push_back(entry);
                if (!Is(",") && !Is(";")) break; Advance();
            }
            const auto close = Match("}"); return Add(SyntaxExpression::Kind::Table, "{}", Cover(token.span, close.span), Link(mark));
        }
        if (token.text == "null") return Add(SyntaxExpression::Kind::Null, token.text, token.span);
        if (token.text == "true" || token.text == "false") return Add(SyntaxExpression::Kind::Boolean, token.text, token.span);
        if (token.kind == SyntaxTokenKind::Number) return Add(SyntaxExpression::Kind::Number, token.text, token.span);
        if (token.kind == SyntaxTokenKind::String) return Add(SyntaxExpression::Kind::String, token.text, token.span);
        if (token.kind != SyntaxTokenKind::Identifier) tree_.diagnostics.push_back({"Expected expression", token.span});
        return Add(token.kind == SyntaxTokenKind::Identifier ? SyntaxExpression::Kind::Identifier : SyntaxExpression::Kind::Missing, token.text, token.span);
    }
    // Parses "(a, b, ...)" and pushes the argument ids on scratch_. Returns their count.
    size_t PushArguments() {
        Match("("); size_t count{};
        while (!AtEnd() && !Is(")")) {
            const auto argument = ParseExpression(); scratch_.push_back(argument); ++count;
            if (!Is(",")) break; Advance();
        }
        Match(")"); return count;
    }
    NodeId ParsePostfix(NodeId value) {
        while (!AtEnd()) {
            const auto start = Span(value);
            if (Is(".")) {
                Advance(); const auto name = MemberName(); value = Add(SyntaxExpression::Kind::Member, name.text, Cover(start, name.span), Link({value}));
            } else if (Is("(")) {
                const auto mark = scratch_.size(); scratch_.push_back(value);
                const auto span = PushArguments() ? Cover(start, Span(scratch_.back())) : start;
                value = Add(SyntaxExpression::Kind::Call, {}, span, Link(mark));
            } else if (Is("[")) {
                Advance(); const auto first = ParseExpression();
                if (Is(":")) { Advance(); const auto last = ParseExpression(); const auto close = Match("]"); value = Add(SyntaxExpression::Kind::Slice, {}, Cover(start, close.span), Link({value, first, last})); }
                else { const auto close = Match("]"); value = Add(SyntaxExpression::Kind::Index, {}, Cover(start, close.span), Link({value, first})); }
            } else break;
        }
        return value;
    }
    TextSpan PreviousSpan() const { return index_ == 0 ? TextSpan{} : tree_.tokens[index_ - 1].span; }

    SyntaxTree tree_;
    std::vector<NodeId> scratch_;
    size_t sourceLength_{};
    size_t index_{};
};

std::string LuaType(const std::string& type) {
    if (type == "int") return "integer"; if (type == "float") return "number"; if (type == "bool") return "boolean"; return type;
}

std::string DefaultValue(const std::string& type, const std::unordered_set<std::string>* enumTypes = nullptr) {
    if (enumTypes && enumTypes->contains(type)) return "0";
    if (const auto builtin = BuiltinDefaultValue(type)) return *builtin;
    return "nil";
}

struct Emitter {
    const Analysis& analysis;
    std::string output;
    std::vector<SourceMapEntry> sourceMap;
    std::unordered_set<std::string> enumTypes;
    size_t nextLabel{};
    size_t lines{};
    void Raw(std::string_view text) { output.append(text); lines += static_cast<size_t>(std::count(text.begin(), text.end(), '\n')); }
    void Line(std::string_view text, size_t sourceOffset) {
        sourceMap.push_back({1 + lines, sourceOffset}); Raw(text); Raw("\n");
    }
    const BoundProgram& Program() const { return analysis.bound; }
    const BoundExpression& Expression(NodeId id) const { return Program().Expression(id); }
    const BoundExpression& Child(const BoundExpression& value, size_t index) const { return Program().Child(value, index); }
};

void EmitStatements(Emitter& emitter, NodeRange statements, const std::string& indent, const std::string& continueLabel = {});

std::string EmitExpression(const Emitter& emitter, const BoundExpression& value) {
    const auto child = [&](size_t index) { return EmitExpression(emitter, emitter.Child(value, index)); };
    const auto lowering = value.lowering;
    const auto& text = value.Text();
    const size_t childCount = value.children.count;
    switch (value.kind) {
    case SyntaxExpression::Kind::Missing: return {};
    case SyntaxExpression::Kind::Identifier:
        if (text == "this") return "self";
        return lowering == Lowering::ImplicitField || lowering == Lowering::ImplicitMethod ? "self." + text : text;
    case SyntaxExpression::Kind::Number: case SyntaxExpression::Kind::String:
    case SyntaxExpression::Kind::Boolean: return text;
    case SyntaxExpression::Kind::Table: { std::string out = "{"; for (size_t i{}; i < childCount; ++i) { if (i) out += ", "; out += child(i); } return out + "}"; }
    case SyntaxExpression::Kind::TableEntry:
        return text.empty() ? "[" + child(0) + "] = " + child(1) : text + " = " + child(0);
    case SyntaxExpression::Kind::Null: return "nil";
    case SyntaxExpression::Kind::Parenthesized: return "(" + child(0) + ")";
    case SyntaxExpression::Kind::New: { std::string out = text + ".new("; for (size_t i{}; i < childCount; ++i) { if (i) out += ", "; out += child(i); } return out + ")"; }
    case SyntaxExpression::Kind::Member: return child(0) + "." + text;
    case SyntaxExpression::Kind::Call: {
        const auto& callee = emitter.Child(value, 0); std::string out;
        const auto receiver = [&]() { return callee.kind == SyntaxExpression::Kind::Member ? EmitExpression(emitter, emitter.Child(callee, 0)) : std::string{}; };
        const auto argument = [&](size_t index) { return child(index + 1); };
        switch (lowering) {
        case Lowering::Printf: { std::string result = "io.write(string.format("; for (size_t i = 1; i < childCount; ++i) { if (i != 1) result += ", "; result += child(i); } return result + "))"; }
        case Lowering::ListPushBack: return "table.insert(" + receiver() + ", " + argument(0) + ")";
        case Lowering::ListPopBack: return "table.remove(" + receiver() + ")";
        case Lowering::ListSize: case Lowering::StringLength: return "#(" + receiver() + ")";
        case Lowering::ListEmpty: return "#(" + receiver() + ") == 0";
        case Lowering::ListClear: return "(function(t) for k in pairs(t) do t[k] = nil end end)(" + receiver() + ")";
        case Lowering::StringContains: return "string.find(" + receiver() + ", " + argument(0) + ", 1, true) ~= nil";
        case Lowering::StringStartsWith: return "(function(s, p) return string.sub(s, 1, #p) == p end)(" + receiver() + ", " + argument(0) + ")";
        case Lowering::StringEndsWith: return "(function(s, p) return p == '' or string.sub(s, -#p) == p end)(" + receiver() + ", " + argument(0) + ")";
        case Lowering::StringSubstring: return childCount == 3
            ? "(function(s, i, n) return string.sub(s, i + 1, i + n) end)(" + receiver() + ", " + argument(0) + ", " + argument(1) + ")"
            : "string.sub(" + receiver() + ", " + argument(0) + " + 1)";
        case Lowering::StringLower: return "string.lower(" + receiver() + ")";
        case Lowering::StringUpper: return "string.upper(" + receiver() + ")";
        case Lowering::CastIdentity: return "(" + argument(0) + ")";
        case Lowering::CastString: return "tostring(" + argument(0) + ")";
        case Lowering::CastBoolNumber: return "(" + argument(0) + " ~= 0)";
        case Lowering::CastBoolString: return "(" + argument(0) + " ~= '')";
        case Lowering::CastIntFloat: return "(function(v) return v < 0 and math.ceil(v) or math.floor(v) end)(" + argument(0) + ")";
        case Lowering::CastNumberBool: return "(" + argument(0) + " and 1 or 0)";
        case Lowering::CastNumberString: return "tonumber(" + argument(0) + ")";
        case Lowering::CastDynamic: return value.type == TypeTable::Bool ? "(not not " + argument(0) + ")" : "(" + argument(0) + ")";
        default: break;
        }
        if (callee.lowering == Lowering::ImplicitMethod) out = "self:" + callee.Text();
        else if (callee.kind == SyntaxExpression::Kind::Member) out = EmitExpression(emitter, emitter.Child(callee, 0)) + (lowering == Lowering::StaticCall ? "." : ":") + callee.Text();
        else out = EmitExpression(emitter, callee);
        out += "("; for (size_t i = 1; i < childCount; ++i) { if (i != 1) out += ", "; out += child(i); } return out + ")";
    }
    case SyntaxExpression::Kind::Index:
        if (lowering == Lowering::ListIndex) return "(function(a, i) if i < 0 then i = #a + i end return a[i + 1] end)(" + child(0) + ", " + child(1) + ")";
        return child(0) + "[" + child(1) + "]";
    case SyntaxExpression::Kind::Slice: return "(function(a, s, e) local r = {} if s < 0 then s = #a + s end if e < 0 then e = #a + e end for i = s + 1, e do r[#r + 1] = a[i] end return r end)(" + child(0) + ", " + child(1) + ", " + child(2) + ")";
    case SyntaxExpression::Kind::Range: return child(0) + ", " + child(1);
    case SyntaxExpression::Kind::Lambda: {
        std::string out = "function(";
        const auto parameters = emitter.analysis.syntax.Parameters(*value.syntax);
        for (size_t i{}; i < parameters.size(); ++i) { if (i) out += ", "; out += parameters[i].name; }
        if (!value.LambdaIsBlock()) return out + ") return " + child(0) + " end";
        Emitter nested{emitter.analysis}; EmitStatements(nested, value.lambdaBody, "    "); return out + ")\n" + nested.output + "end";
    }
    case SyntaxExpression::Kind::Unary: return (text == "!" ? "not " : text) + child(0);
    case SyntaxExpression::Kind::Assignment: {
        if (lowering == Lowering::ListSet) {
            const auto& index = emitter.Child(value, 0); const std::string slot = "a[i + 1]";
            const std::string operation = text == "+=" && value.type == TypeTable::String ? ".." : text.substr(0, 1);
            const std::string assignment = text == "=" ? slot + " = v" : slot + " = " + slot + " " + operation + " v";
            return "(function(a, i, v) if i < 0 then i = #a + i end " + assignment + " end)(" + EmitExpression(emitter, emitter.Child(index, 0)) + ", " + EmitExpression(emitter, emitter.Child(index, 1)) + ", " + child(1) + ")";
        }
        if (text.size() == 2 && text[1] == '=') return child(0) + " = " + child(0) + " " + (lowering == Lowering::CompoundConcat ? ".." : text.substr(0, 1)) + " " + child(1);
        return child(0) + " = " + child(1);
    }
    case SyntaxExpression::Kind::Binary: { const auto& op = lowering == Lowering::Concat ? ".." : text == "!=" ? "~=" : text == "&&" ? "and" : text == "||" ? "or" : text; return child(0) + " " + op + " " + child(1); }
    }
    return {};
}

void EmitIf(Emitter& emitter, const BoundStatement& first, const std::string& indent, const std::string& continueLabel) {
    const BoundStatement* current = &first; std::string keyword = "if";
    for (;;) {
        emitter.Line(indent + keyword + " " + EmitExpression(emitter, emitter.Expression(current->expression)) + " then", current->span.offset);
        EmitStatements(emitter, current->body, indent + "    ", continueLabel);
        const auto elseBody = emitter.Program().Statements(current->elseBody);
        if (elseBody.size() == 1 && elseBody.front().kind == SyntaxStatement::Kind::If) { current = &elseBody.front(); keyword = "elseif"; continue; }
        if (!elseBody.empty()) { emitter.Line(indent + "else", current->span.offset); EmitStatements(emitter, current->elseBody, indent + "    ", continueLabel); }
        emitter.Line(indent + "end", first.span.offset); return;
    }
}

void EmitStatements(Emitter& emitter, NodeRange statements, const std::string& indent, const std::string& continueLabel) {
    for (const auto& statement : emitter.Program().Statements(statements)) {
        const auto& expression = emitter.Expression(statement.expression);
        switch (statement.kind) {
        case SyntaxStatement::Kind::If: EmitIf(emitter, statement, indent, continueLabel); break;
        case SyntaxStatement::Kind::While: {
            const auto label = "__luasharp_continue_" + std::to_string(emitter.nextLabel++);
            emitter.Line(indent + "while " + EmitExpression(emitter, expression) + " do", statement.span.offset);
            emitter.Line(indent + "    do", statement.span.offset);
            EmitStatements(emitter, statement.body, indent + "        ", label);
            emitter.Line(indent + "    end", statement.span.offset);
            emitter.Line(indent + "    ::" + label + "::", statement.span.offset);
            emitter.Line(indent + "end", statement.span.offset); break;
        }
        case SyntaxStatement::Kind::For: {
            const auto label = "__luasharp_continue_" + std::to_string(emitter.nextLabel++);
            const auto& increment = emitter.Expression(statement.increment);
            emitter.Line(indent + "do", statement.span.offset);
            EmitStatements(emitter, statement.initializer, indent + "    ");
            emitter.Line(indent + "    while " + EmitExpression(emitter, expression) + " do", statement.span.offset);
            emitter.Line(indent + "        do", statement.span.offset);
            EmitStatements(emitter, statement.body, indent + "            ", label);
            emitter.Line(indent + "        end", statement.span.offset);
            emitter.Line(indent + "        ::" + label + "::", statement.span.offset);
            if (increment.kind != SyntaxExpression::Kind::Missing) emitter.Line(indent + "        " + EmitExpression(emitter, increment), increment.span.offset);
            emitter.Line(indent + "    end", statement.span.offset);
            emitter.Line(indent + "end", statement.span.offset); break;
        }
        case SyntaxStatement::Kind::ForEach: {
            const auto loopId = emitter.nextLabel++; const auto label = "__luasharp_continue_" + std::to_string(loopId); std::string header; std::string listIndex;
            const auto& name = statement.Name(); const auto& secondName = statement.SecondName();
            if (expression.kind == SyntaxExpression::Kind::Range) {
                header = "for " + name + " = " + EmitExpression(emitter, expression) + " do";
            } else if (!secondName.empty() && emitter.analysis.types.IsList(expression.type)) {
                listIndex = "__luasharp_index_" + std::to_string(loopId);
                header = "for " + listIndex + ", " + secondName + " in ipairs(" + EmitExpression(emitter, expression) + ") do";
            } else if (!secondName.empty()) {
                header = "for " + name + ", " + secondName + " in pairs(" + EmitExpression(emitter, expression) + ") do";
            } else {
                header = "for _, " + name + " in ipairs(" + EmitExpression(emitter, expression) + ") do";
            }
            emitter.Line(indent + header, statement.span.offset);
            if (!listIndex.empty()) emitter.Line(indent + "    local " + name + " = " + listIndex + " - 1", statement.span.offset);
            emitter.Line(indent + "    do", statement.span.offset);
            EmitStatements(emitter, statement.body, indent + "        ", label);
            emitter.Line(indent + "    end", statement.span.offset);
            emitter.Line(indent + "    ::" + label + "::", statement.span.offset);
            emitter.Line(indent + "end", statement.span.offset); break;
        }
        case SyntaxStatement::Kind::Break: emitter.Line(indent + "break", statement.span.offset); break;
        case SyntaxStatement::Kind::Continue: emitter.Line(indent + "goto " + continueLabel, statement.span.offset); break;
        case SyntaxStatement::Kind::Return: emitter.Line(indent + "return" + (expression.kind == SyntaxExpression::Kind::Missing ? "" : " " + EmitExpression(emitter, expression)), statement.span.offset); break;
        case SyntaxStatement::Kind::Local: emitter.Line(indent + "local " + statement.Name() + " = " + (expression.kind == SyntaxExpression::Kind::Missing ? DefaultValue(emitter.analysis.types.Name(statement.type), &emitter.enumTypes) : EmitExpression(emitter, expression)), statement.span.offset); break;
        case SyntaxStatement::Kind::Expression: emitter.Line(indent + EmitExpression(emitter, expression), statement.span.offset); break;
        }
    }
}

void EmitClass(Emitter& emitter, const BoundClass& value) {
    emitter.Raw("---@class " + value.name + (value.baseName.empty() ? "" : " : " + value.baseName) + "\n");
    for (const auto& field : value.fields) {
        const auto initializer = emitter.Expression(field.initializer).kind;
        emitter.Raw("---@field " + field.access + " " + field.name + " " + LuaType(field.type));
        if (initializer == SyntaxExpression::Kind::Null || (initializer == SyntaxExpression::Kind::Missing && DefaultValue(field.type, &emitter.enumTypes) == "nil")) emitter.Raw("?"); emitter.Raw("\n");
    }
    emitter.Raw("local " + value.name + " = {}\n" + value.name + ".__index = " + value.name + "\n");
    if (!value.baseName.empty()) emitter.Raw("setmetatable(" + value.name + ", { __index = " + value.baseName + " })\n");
    emitter.Raw("\n");
    const auto constructor = std::find_if(value.functions.begin(), value.functions.end(), [](const auto& f) { return f.isConstructor; });
    emitter.Raw("function " + value.name + ".new(");
    if (constructor != value.functions.end()) for (size_t i{}; i < constructor->parameters.size(); ++i) { if (i) emitter.Raw(", "); emitter.Raw(constructor->parameters[i].name); }
    emitter.Raw(")\n    local self = setmetatable(" + (value.baseName.empty() ? std::string("{}") : value.baseName + ".new()") + ", " + value.name + ")\n");
    for (const auto& field : value.fields) {
        const auto& initializer = emitter.Expression(field.initializer);
        emitter.Line("    self." + field.name + " = " + (initializer.kind == SyntaxExpression::Kind::Missing ? DefaultValue(field.type, &emitter.enumTypes) : EmitExpression(emitter, initializer)), field.span.offset);
    }
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

void EmitEnum(Emitter& emitter, const BoundEnum& value) {
    emitter.Raw("---@enum " + value.name + "\nlocal " + value.name + " = {\n");
    for (const auto& member : value.members) emitter.Line("    " + member.name + " = " + std::to_string(member.value) + ",", member.span.offset);
    emitter.Raw("}\n\n");
}

std::runtime_error SyntaxError(const SyntaxDiagnostic& diagnostic) {
    return std::runtime_error(diagnostic.message + " at " + std::to_string(diagnostic.span.offset));
}

} // namespace

SyntaxTree ParseTree(std::string_view source) {
    return Parser(Lexer(source).Run(), source.size()).Run();
}

SyntaxTree ParseSyntax(std::string_view source) {
    auto tree = ParseTree(source); if (!tree.diagnostics.empty()) throw SyntaxError(tree.diagnostics.front()); return tree;
}

ExpressionTree ParseExpression(const std::vector<SyntaxToken>& tokens) {
    SyntaxTree lexed; lexed.tokens = tokens;
    const size_t end = tokens.empty() ? 0 : tokens.back().span.End();
    lexed.tokens.push_back({SyntaxTokenKind::EndOfFile, {}, {end, 0}});
    Parser parser(std::move(lexed), end); const auto root = parser.RunExpression();
    if (!parser.Diagnostics().empty()) throw SyntaxError(parser.Diagnostics().front());
    return {parser.Take(), root};
}

std::string Compile(std::string_view source) { return CompileWithSourceMap(source).lua; }

CompilationResult CompileWithSourceMap(std::string_view source) {
    auto analysis = Analyze(source); auto& tree = analysis.syntax; if (!tree.diagnostics.empty()) throw SyntaxError(tree.diagnostics.front());
    const auto error = std::find_if(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.severity == DiagnosticSeverity::Error; });
    if (error != analysis.diagnostics.end()) throw std::runtime_error(error->message + " at " + std::to_string(error->span.offset));
    Emitter emitter{analysis}; for (const auto& value : analysis.bound.enums) emitter.enumTypes.insert(value.name); for (const auto& value : analysis.bound.enums) EmitEnum(emitter, value); for (const auto& value : analysis.bound.classes) EmitClass(emitter, value); EmitStatements(emitter, analysis.bound.topLevel, {});
    return {std::move(emitter.output), std::move(emitter.sourceMap)};
}

} // namespace luasharp
