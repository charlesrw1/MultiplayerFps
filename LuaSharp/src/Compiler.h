#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
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

// Syntax and bound trees store nodes in flat pools and link them by index.
using NodeId = uint32_t;
struct NodeRange { NodeId first{}; NodeId count{}; };

// Nodes of a pool, listed by id. Iterates as references.
template <typename T> class NodeList {
public:
    NodeList(const std::vector<T>& pool, std::span<const NodeId> ids) : pool_(&pool), ids_(ids) {}
    struct Iterator {
        const std::vector<T>* pool;
        const NodeId* at;
        const T& operator*() const { return (*pool)[*at]; }
        Iterator& operator++() { ++at; return *this; }
        bool operator==(const Iterator&) const = default;
    };
    [[nodiscard]] Iterator begin() const { return {pool_, ids_.data()}; }
    [[nodiscard]] Iterator end() const { return {pool_, ids_.data() + ids_.size()}; }
    [[nodiscard]] size_t size() const { return ids_.size(); }
    [[nodiscard]] bool empty() const { return ids_.empty(); }
    [[nodiscard]] const T& operator[](size_t index) const { assert(index < ids_.size()); return (*pool_)[ids_[index]]; }
    [[nodiscard]] const T& front() const { return (*this)[0]; }

private:
    const std::vector<T>* pool_;
    std::span<const NodeId> ids_;
};

struct SyntaxParameter { std::string type; std::string name; TextSpan span; };
// Ranges index SyntaxTree::links (child ids) or SyntaxTree::parameters.
struct SyntaxExpression {
    enum class Kind {
        Missing, Identifier, Number, String, Null, Boolean, Table, TableEntry, New,
        Member, Call, Index, Slice, Range, Lambda, Unary, Binary, Assignment, Parenthesized,
        Is, As // text is the type name; one child, the operand
    } kind{Kind::Missing};
    std::string text;
    TextSpan span;
    NodeRange children;   // expression ids
    NodeRange parameters; // lambda parameters
    NodeRange lambdaBody; // statement ids
    bool lambdaIsBlock{};
};

struct SyntaxField {
    std::string access;
    std::string type;
    std::string name;
    NodeId initializer{}; // 0 is the shared missing expression
    TextSpan span;
    bool isStatic{};
};
struct SyntaxStatement {
    enum class Kind { If, While, For, ForEach, Break, Continue, Return, Local, Expression } kind{Kind::Expression};
    std::string name;
    std::string secondName;
    std::string type;
    TextSpan nameSpan;
    TextSpan secondNameSpan;
    bool inferredType{};
    NodeId expression{}; // 0 is the shared missing expression
    NodeId increment{};
    NodeRange initializer; // statement ids
    NodeRange body;
    NodeRange elseBody;
    TextSpan span;
};
struct SyntaxModifiers {
    bool isStatic{}, isVirtual{}, isOverride{}, isAbstract{}, isSealed{};
};
struct SyntaxFunction {
    std::string access;
    std::string name;
    std::string returnType;
    bool isConstructor{};
    bool isStatic{};
    SyntaxModifiers modifiers;
    bool hasBody{true};      // false for `abstract void f();`
    bool hasBaseCall{};      // constructor declares `: base(...)`
    NodeRange baseArguments; // expression ids
    TextSpan baseSpan;
    std::vector<SyntaxParameter> parameters;
    NodeRange body;
    TextSpan span;
};
// Classes and structs share one syntax node; a struct is a value type.
struct SyntaxClass {
    std::string name;
    std::string baseName;
    bool isStruct{};
    bool isAbstract{};
    bool isSealed{};
    bool isExtern{}; // declared by the host: members have no bodies
    std::vector<SyntaxField> fields;
    std::vector<SyntaxFunction> functions;
    TextSpan span;
};
struct SyntaxEnumMember {
    std::string name;
    TextSpan nameSpan;
    NodeId initializer{};
    TextSpan span;
};
struct SyntaxEnum {
    std::string name;
    TextSpan nameSpan;
    std::vector<SyntaxEnumMember> members;
    TextSpan span;
};
struct SyntaxProgram {
    std::vector<SyntaxEnum> enums;
    std::vector<SyntaxClass> classes;
    NodeRange topLevel;
    TextSpan span;
};
// Nodes never move after parsing, so pointers into the pools stay valid while the tree lives.
struct SyntaxTree {
    SyntaxProgram root;
    std::vector<SyntaxToken> tokens;
    std::vector<SyntaxDiagnostic> diagnostics;
    std::vector<SyntaxExpression> expressions; // [0] is the shared missing expression
    std::vector<SyntaxStatement> statements;
    std::vector<SyntaxParameter> parameters;
    std::vector<NodeId> links;

    [[nodiscard]] const SyntaxExpression& Expression(NodeId id) const { assert(id < expressions.size()); return expressions[id]; }
    [[nodiscard]] NodeList<SyntaxExpression> Children(const SyntaxExpression& value) const { return {expressions, Links(value.children)}; }
    [[nodiscard]] const SyntaxExpression& Child(const SyntaxExpression& value, size_t index) const { assert(index < value.children.count); return Expression(links[value.children.first + index]); }
    [[nodiscard]] NodeList<SyntaxStatement> Statements(NodeRange range) const { return {statements, Links(range)}; }
    [[nodiscard]] NodeList<SyntaxExpression> Expressions(NodeRange range) const { return {expressions, Links(range)}; }
    [[nodiscard]] std::span<const SyntaxParameter> Parameters(const SyntaxExpression& value) const {
        assert(value.parameters.first + value.parameters.count <= parameters.size());
        return std::span(parameters).subspan(value.parameters.first, value.parameters.count);
    }

private:
    [[nodiscard]] std::span<const NodeId> Links(NodeRange range) const {
        assert(range.first + range.count <= links.size());
        return std::span(links).subspan(range.first, range.count);
    }
};
struct ExpressionTree {
    SyntaxTree tree;
    NodeId root{};
    [[nodiscard]] const SyntaxExpression& Root() const { return tree.Expression(root); }
};

// ParseTree is error-tolerant and is the entry point for editor tooling.
SyntaxTree ParseTree(std::string_view source);
// Helpers for callers that require valid syntax; they throw on the first syntax error.
SyntaxTree ParseSyntax(std::string_view source);
ExpressionTree ParseExpression(const std::vector<SyntaxToken>& tokens);

} // namespace luasharp
