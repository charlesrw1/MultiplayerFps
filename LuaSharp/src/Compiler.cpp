#include "Compiler.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace luasharp {
namespace {

const std::unordered_set<std::string> Keywords{
    "class", "struct", "static", "enum", "public", "private", "auto", "new", "if", "else", "while", "for", "in", "break", "continue", "return",
    "null", "true", "false", "virtual", "override", "abstract", "sealed", "is", "as", "extern"
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
            if (Is("class") || Is("struct") || Is("abstract") || Is("sealed") || Is("extern")) { auto value = ParseClass(); tree_.root.classes.push_back(std::move(value)); }
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
    // The attributes between `[` and `]`.
    void ParseAttributes(std::vector<SyntaxAttribute>& attributes) {
        do {
            const auto name = Identifier("attribute name");
            SyntaxAttribute attribute{name.text, name.span};
            if (Is("(")) {
                Advance();
                const auto& argument = Current();
                // A plain name in quotes; escapes are not decoded.
                if (argument.kind == SyntaxTokenKind::String && argument.text.size() >= 2 && argument.text.find('\\') == std::string::npos)
                    attribute.argument = Advance().text.substr(1, argument.text.size() - 2);
                else
                    tree_.diagnostics.push_back({"Expected a string literal without escapes as the attribute argument", argument.span});
                Match(")");
            }
            attributes.push_back(std::move(attribute));
        } while (Is(",") && (Advance(), true));
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
    SyntaxFunction ParseFunction(std::string access, SyntaxToken name, std::string returnType, bool constructor, SyntaxModifiers modifiers) {
        SyntaxFunction result; result.access = std::move(access); result.name = name.text; result.returnType = std::move(returnType); result.isConstructor = constructor; result.span = name.span;
        result.modifiers = modifiers; result.isStatic = modifiers.isStatic;
        Match("(");
        while (!AtEnd() && !Is(")")) {
            auto [type, typeSpan] = ParseType(); const auto parameterName = Identifier("parameter name");
            result.parameters.push_back({std::move(type), parameterName.text, Cover(typeSpan, parameterName.span)});
            if (!Is(",")) break; Advance();
        }
        Match(")");
        if (constructor && Is(":")) {
            const auto colon = Advance().span;
            if (Is("base")) Advance(); else tree_.diagnostics.push_back({"Expected 'base' after ':' in a constructor", Current().span});
            const auto mark = scratch_.size(); PushArguments();
            result.hasBaseCall = true; result.baseArguments = Link(mark); result.baseSpan = Cover(colon, PreviousSpan());
        }
        if (Is(";")) { Advance(); result.hasBody = false; }
        else result.body = ParseBlock();
        return result;
    }
    // static, virtual, override, abstract and sealed, in any order.
    SyntaxModifiers ParseModifiers() {
        SyntaxModifiers result;
        for (;;) {
            bool* flag = Is("static") && !Is("(", 1) ? &result.isStatic : Is("virtual") ? &result.isVirtual : Is("override") ? &result.isOverride
                : Is("abstract") ? &result.isAbstract : Is("sealed") ? &result.isSealed : nullptr;
            if (!flag) return result;
            if (*flag) tree_.diagnostics.push_back({"Duplicate modifier '" + Current().text + "'", Current().span});
            *flag = true; Advance();
        }
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
        const bool isExtern = Is("extern"); if (isExtern) Advance();
        const auto classModifiers = ParseModifiers();
        const bool isStruct = Is("struct"); if (!isStruct) Match("class"); else Advance();
        const auto name = Identifier(isStruct ? "struct name" : "class name"); SyntaxClass result; result.name = name.text; result.span = name.span; result.isStruct = isStruct;
        result.isAbstract = classModifiers.isAbstract; result.isSealed = classModifiers.isSealed; result.isExtern = isExtern;
        if (isExtern && isStruct) tree_.diagnostics.push_back({"Only a class can be 'extern'", name.span});
        if (classModifiers.isStatic || classModifiers.isVirtual || classModifiers.isOverride) tree_.diagnostics.push_back({"Only 'abstract' and 'sealed' can modify a class", name.span});
        if (Is(":")) {
            Advance(); result.baseName = Identifier("base type").text;
            if (isStruct) tree_.diagnostics.push_back({"Structs cannot have a base type", name.span});
        }
        Match("{"); std::string access = "public";
        while (!AtEnd() && !Is("}")) {
            if ((Is("public") || Is("private")) && Is(":", 1)) { access = Advance().text; Advance(); continue; }
            const size_t before = index_;
            std::vector<SyntaxAttribute> attributes;
            while (Is("[")) { Advance(); ParseAttributes(attributes); Match("]"); }
            const auto modifiers = ParseModifiers();
            const auto first = Identifier();
            const auto rejectAttributes = [&] { for (const auto& attribute : attributes) tree_.diagnostics.push_back({"Only fields can have attributes", attribute.span}); };
            if (Is("(")) {
                rejectAttributes();
                result.functions.push_back(ParseFunction(access, first, {}, true, modifiers));
                if (modifiers.isStatic || modifiers.isVirtual || modifiers.isOverride || modifiers.isAbstract || modifiers.isSealed) tree_.diagnostics.push_back({"Constructors cannot have modifiers", first.span});
            } else {
                std::string type = first.text;
                while (Is("[")) { Advance(); Match("]"); type += "[]"; }
                const auto memberName = Identifier("member name");
                if (Is("(")) { rejectAttributes(); result.functions.push_back(ParseFunction(access, memberName, std::move(type), false, modifiers)); }
                else {
                    if (modifiers.isVirtual || modifiers.isOverride || modifiers.isAbstract || modifiers.isSealed) tree_.diagnostics.push_back({"Fields can only be 'static'", memberName.span});
                    SyntaxField field; field.access = access; field.type = std::move(type); field.name = memberName.text; field.span = memberName.span; field.isStatic = modifiers.isStatic;
                    field.attributes = std::move(attributes);
                    if (Is("=")) { Advance(); field.initializer = ParseExpression(); }
                    Match(";"); result.fields.push_back(std::move(field));
                }
            }
            if (before == index_) Advance();
        }
        Match("}"); return result;
    }

    static constexpr int TypeTestPrecedence = 5; // `is` and `as` bind like relational operators
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
            const auto& operation = Current();
            if (operation.kind == SyntaxTokenKind::Keyword && (operation.text == "is" || operation.text == "as")) {
                if (TypeTestPrecedence < minimum) break;
                const auto kind = Advance().text == "is" ? SyntaxExpression::Kind::Is : SyntaxExpression::Kind::As;
                auto [type, span] = ParseType();
                left = Add(kind, std::move(type), Cover(Span(left), span), Link({left}));
                continue;
            }
            const int precedence = Precedence(operation.text);
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

} // namespace luasharp
