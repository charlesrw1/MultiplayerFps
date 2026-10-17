#pragma once

#include "Compiler.h"

#include <cassert>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace luasharp {

struct BuiltinMember;
struct BuiltinOperator;

struct StringHash {
    using is_transparent = void;
    size_t operator()(std::string_view value) const { return std::hash<std::string_view>{}(value); }
};
template <typename T> using StringMap = std::unordered_map<std::string, T, StringHash, std::equal_to<>>;
using StringSet = std::unordered_set<std::string, StringHash, std::equal_to<>>;

using TypeId = uint32_t;
enum class TypeKind : uint8_t { Unknown, Void, Null, Any, Table, Int, Float, Bool, String, Range, Named, List, Function, Meta, Builtin };

// Interns every type the analyzer sees. Name() returns the canonical spelling:
// "int", "Item[]", "$function:<arity>:<result>", "$type:<name>", "$builtin:<name>", "" for unknown.
class TypeTable {
public:
    static constexpr TypeId Unknown = 0, Void = 1, Null = 2, Any = 3, Table = 4, Int = 5, Float = 6, Bool = 7, String = 8, Range = 9;

    TypeTable();
    TypeId Intern(std::string_view spelling);
    [[nodiscard]] std::optional<TypeId> Find(std::string_view spelling) const;
    TypeId List(TypeId element);
    TypeId Function(size_t arity, TypeId result);
    TypeId Meta(std::string_view name);
    TypeId Builtin(std::string_view name);

    [[nodiscard]] const std::string& Name(TypeId id) const { return At(id).name; }
    [[nodiscard]] TypeKind Kind(TypeId id) const { return At(id).kind; }
    [[nodiscard]] TypeId Count() const { return static_cast<TypeId>(entries_.size()); }
    [[nodiscard]] bool IsList(TypeId id) const { return Kind(id) == TypeKind::List; }
    [[nodiscard]] bool IsFunction(TypeId id) const { return Kind(id) == TypeKind::Function; }
    // List element or function result.
    [[nodiscard]] TypeId Element(TypeId id) const { assert(IsList(id) || IsFunction(id)); return At(id).element; }
    [[nodiscard]] size_t Arity(TypeId id) const { assert(IsFunction(id)); return At(id).arity; }
    // The named type behind a list ("Item[][]" -> "Item"); other types map to themselves.
    [[nodiscard]] TypeId Base(TypeId id) const { while (IsList(id)) id = Element(id); return id; }
    // Bare name of a Meta or Builtin type.
    [[nodiscard]] std::string_view Target(TypeId id) const;

private:
    struct Entry { TypeKind kind{}; std::string name; TypeId element{}; uint32_t arity{}; };
    const Entry& At(TypeId id) const { assert(id < entries_.size()); return entries_[id]; }
    TypeId Add(TypeKind kind, std::string name, TypeId element = {}, uint32_t arity = {});
    TypeId Composite(TypeKind kind, TypeId element, uint32_t arity, std::string name);

    std::vector<Entry> entries_;
    StringMap<TypeId> byName_;
    std::unordered_map<uint64_t, TypeId> composites_; // (kind, element, arity) -> id
};

enum class Lowering : uint8_t {
    None, ImplicitField, ImplicitMethod, StaticCall, StaticField, Base, NativeMethod, NativeStatic, NativeConstant, Construct, Printf, Concat, CompoundConcat, ListIndex, ListSet,
    ListPushBack, ListPopBack, ListSize, ListEmpty, ListClear,
    StringLength, StringContains, StringStartsWith, StringEndsWith, StringSubstring, StringLower, StringUpper,
    CastIdentity, CastString, CastBoolNumber, CastBoolString, CastIntFloat, CastNumberBool, CastNumberString, CastDynamic
};
std::string_view LoweringName(Lowering lowering);

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
    bool implicit{}; // member of a builtin or external type; has no declaration in this source
    bool isStatic{};
    bool isStruct{}; // Class symbols: a value type
    bool isVirtual{};  // Methods: dispatched through the vtable (virtual, abstract or override)
    bool isOverride{};
    bool isAbstract{}; // Methods without a body; Class symbols that cannot be constructed
    bool isSealed{};   // Methods that cannot be overridden; Class symbols that cannot be derived from
    bool isNative{};   // Class symbols: an `extern class`; its objects are host proxies (LS_NativeObject)
};
struct ExternalType {
    std::string name;
    std::string baseType;
    std::vector<Symbol> members;
    bool isEnum{};
    bool isStruct{};
    bool isNative{}; // an `extern class`: the host implements its members (see docs/native-abi.md)
    std::vector<long long> values; // enums: the value of each member; empty means ordinals
};
enum class DiagnosticSeverity { Error, Warning };
struct Diagnostic {
    std::string message;
    size_t line{};
    size_t character{};
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    TextSpan span;
};
// name views the syntax tree or a literal; valid while the owning Analysis lives.
struct VariableBinding {
    std::string_view name;
    TypeId type{};
    TextSpan declaration;
    TextSpan visibility;
    size_t depth{};
    size_t symbolIndex{};
};

// Bound nodes live in the BoundProgram pools and link by index. They point at
// their syntax node for text, so they stay trivially destructible.
struct BoundExpression {
    SyntaxExpression::Kind kind{SyntaxExpression::Kind::Missing};
    Lowering lowering{};
    TypeId type{};
    TextSpan span;
    std::optional<size_t> symbolIndex;
    NodeRange children;
    NodeRange lambdaBody;
    const SyntaxExpression* syntax{}; // null only for the shared missing node
    const BuiltinMember* nativeMember{};     // call or static constant resolved to a native C function or expression
    const BuiltinOperator* nativeOperator{}; // unary, binary or compound operator implemented by a native C function

    [[nodiscard]] const std::string& Text() const { static const std::string none; return syntax ? syntax->text : none; }
    [[nodiscard]] bool LambdaIsBlock() const { return syntax && syntax->lambdaIsBlock; }
};
struct BoundStatement {
    SyntaxStatement::Kind kind{SyntaxStatement::Kind::Expression};
    bool inferredType{};
    TypeId type{};
    TextSpan span;
    NodeId expression{}; // 0 is the shared missing expression
    NodeId increment{};
    NodeRange initializer;
    NodeRange body;
    NodeRange elseBody;
    const SyntaxStatement* syntax{};

    [[nodiscard]] const std::string& Name() const { assert(syntax); return syntax->name; }
    [[nodiscard]] const std::string& SecondName() const { assert(syntax); return syntax->secondName; }
};
struct BoundFunction {
    std::string access;
    std::string name;
    std::string returnType;
    bool isConstructor{};
    bool isStatic{};
    bool isVirtual{};
    bool isAbstract{};
    size_t symbolIndex{};
    std::vector<SyntaxParameter> parameters;
    NodeRange body;
    // Constructors of derived classes: `: base(...)`. Without it, the parameterless base constructor runs.
    bool hasBaseCall{};
    std::optional<size_t> baseConstructor; // null for the implicit default constructor of the base
    NodeRange baseArguments;               // bound expression ids
    TextSpan span;
};
struct BoundField {
    std::string access;
    std::string type;
    std::string name;
    bool isStatic{};
    bool isEditable{}; // [Editable]: the host can edit and save it
    NodeId initializer{};
    TextSpan span;
};
struct BoundClass {
    std::string name;
    std::string baseName;
    bool isStruct{};
    bool isAbstract{};
    bool isSealed{};
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
    NodeRange topLevel;
    TextSpan span;
    std::vector<BoundExpression> expressions; // [0] is the shared missing expression
    std::vector<BoundStatement> statements;

    [[nodiscard]] const BoundExpression& Expression(NodeId id) const { assert(id < expressions.size()); return expressions[id]; }
    [[nodiscard]] std::span<const BoundExpression> Children(const BoundExpression& value) const { return Slice(expressions, value.children); }
    [[nodiscard]] const BoundExpression& Child(const BoundExpression& value, size_t index) const { assert(index < value.children.count); return expressions[value.children.first + index]; }
    [[nodiscard]] std::span<const BoundStatement> Statements(NodeRange range) const { return Slice(statements, range); }

private:
    template <typename T> static std::span<const T> Slice(const std::vector<T>& pool, NodeRange range) {
        assert(range.first + range.count <= pool.size());
        return std::span<const T>(pool).subspan(range.first, range.count);
    }
};
// Bound nodes point into syntax, so an Analysis can move but not copy.
struct Analysis {
    SyntaxTree syntax;
    BoundProgram bound;
    TypeTable types;
    std::vector<Symbol> symbols;
    std::vector<Diagnostic> diagnostics;
    std::vector<VariableBinding> bindings;
    StringMap<std::vector<size_t>> members; // type -> indices into symbols
    // Compatibility summary for existing consumers. Scoped queries should use
    // ResolveVariableType instead.
    StringMap<TypeId> variableTypes;
    StringMap<std::string> baseTypes;

    Analysis() = default;
    Analysis(Analysis&&) = default;
    Analysis& operator=(Analysis&&) = default;
    Analysis(const Analysis&) = delete;
    Analysis& operator=(const Analysis&) = delete;
};

Analysis Analyze(std::string_view source);
Analysis Analyze(std::string_view source, const std::vector<ExternalType>& externalTypes);
// The classes and enums an analysis declares, as other sources see them. Inherited members are included.
std::vector<ExternalType> ExportTypes(const Analysis& analysis);
std::optional<std::string> ResolveVariableType(const Analysis& analysis, std::string_view name, size_t position);
// Innermost bound expression whose span contains position.
const BoundExpression* FindExpression(const Analysis& analysis, size_t position);
const Symbol* FindMember(const Analysis& analysis, std::string_view type, std::string_view name);

} // namespace luasharp
