#include "Builtins.h"

#include <algorithm>
#include <map>
#include <mutex>

namespace luasharp {
namespace {

using P = BuiltinParameter;
using M = BuiltinMember;

const std::vector<std::string> NativeStructs{"Vec2", "Vec3", "Vec2i", "Vec3i", "Quat", "Mat4x4"};
const std::vector<std::string> Types{"int", "float", "bool", "string", "table", "Vec2", "Vec3", "Vec2i", "Vec3i", "Quat", "Mat4x4"};

const std::vector<M> Members{
    {"string", "length", "int", {}, SymbolKind::Method, false, Lowering::StringLength},
    {"string", "size", "int", {}, SymbolKind::Method, false, Lowering::StringLength},
    {"string", "contains", "bool", {{"string", "value"}}, SymbolKind::Method, false, Lowering::StringContains},
    {"string", "starts_with", "bool", {{"string", "prefix"}}, SymbolKind::Method, false, Lowering::StringStartsWith},
    {"string", "ends_with", "bool", {{"string", "suffix"}}, SymbolKind::Method, false, Lowering::StringEndsWith},
    {"string", "substring", "string", {{"int", "start"}, {"int", "length", true}}, SymbolKind::Method, false, Lowering::StringSubstring},
    {"string", "lower", "string", {}, SymbolKind::Method, false, Lowering::StringLower},
    {"string", "upper", "string", {}, SymbolKind::Method, false, Lowering::StringUpper},


#include "BuiltinMathMembers.inc"
};

const std::vector<BuiltinOperator> Operators{
    {"int", "+", "int", "int", {}}, {"int", "-", "int", "int", {}}, {"int", "*", "int", "int", {}}, {"int", "/", "int", "float", {}}, {"int", "%", "int", "int", {}},
    {"float", "+", "float", "float", {}}, {"float", "-", "float", "float", {}}, {"float", "*", "float", "float", {}}, {"float", "/", "float", "float", {}},
    {"float", "+", "int", "float", {}}, {"float", "-", "int", "float", {}}, {"float", "*", "int", "float", {}}, {"float", "/", "int", "float", {}},
    {"int", "+", "float", "float", {}}, {"int", "-", "float", "float", {}}, {"int", "*", "float", "float", {}}, {"int", "/", "float", "float", {}},
    {"string", "+", "string", "string", Lowering::Concat},

#include "BuiltinMathOperators.inc"
};

const std::vector<BuiltinFunction> Functions{
    {"printf", "void", {{"string", "format"}}, true, Lowering::Printf},
};

std::string ElementType(std::string_view array) { return std::string(array.substr(0, array.size() - 2)); }

} // namespace

const std::vector<std::string>& BuiltinTypes() { return Types; }
bool IsBuiltinType(std::string_view type) { return std::find(Types.begin(), Types.end(), type) != Types.end(); }
const std::vector<std::string>& NativeStructTypes() { return NativeStructs; }
bool IsNativeStruct(std::string_view type) { return std::find(NativeStructs.begin(), NativeStructs.end(), type) != NativeStructs.end(); }
bool IsBuiltinStaticClass(std::string_view type) { return type == "math"; }

namespace {
// Members of a list type, created once per list type. Never erased, so returned pointers stay valid.
const std::vector<BuiltinMember>& ListMembers(std::string_view type) {
    static std::mutex mutex;
    static std::map<std::string, std::vector<BuiltinMember>, std::less<>> lists;
    std::lock_guard lock(mutex);
    auto found = lists.find(type);
    if (found != lists.end()) return found->second;
    const auto element = ElementType(type);
    return lists.emplace(std::string(type), std::vector<BuiltinMember>{
        {std::string(type), "push_back", "void", {{element, "value"}}, SymbolKind::Method, false, Lowering::ListPushBack},
        {std::string(type), "pop_back", element, {}, SymbolKind::Method, false, Lowering::ListPopBack},
        {std::string(type), "size", "int", {}, SymbolKind::Method, false, Lowering::ListSize},
        {std::string(type), "empty", "bool", {}, SymbolKind::Method, false, Lowering::ListEmpty},
        {std::string(type), "clear", "void", {}, SymbolKind::Method, false, Lowering::ListClear},
    }).first->second;
}
} // namespace

std::vector<const BuiltinMember*> BuiltinMembers(std::string_view type, bool staticMembers) {
    std::vector<const BuiltinMember*> result;
    if (type.ends_with("[]")) {
        if (!staticMembers) for (const auto& member : ListMembers(type)) result.push_back(&member);
        return result;
    }
    for (const auto& member : Members) if (member.owner == type && member.isStatic == staticMembers) result.push_back(&member);
    return result;
}

std::vector<const BuiltinMember*> FindBuiltinMembers(std::string_view type, std::string_view name, bool staticMember) {
    std::vector<const BuiltinMember*> result;
    for (const auto* member : BuiltinMembers(type, staticMember)) if (member->name == name) result.push_back(member);
    return result;
}

const BuiltinMember* FindBuiltinMember(std::string_view type, std::string_view name, bool staticMember) {
    const auto found = FindBuiltinMembers(type, name, staticMember);
    return found.empty() ? nullptr : found.front();
}

const BuiltinOperator* FindBuiltinUnary(std::string_view operand, std::string_view operation) {
    const auto found = std::find_if(Operators.begin(), Operators.end(), [&](const auto& value) { return value.right.empty() && value.left == operand && value.operation == operation; });
    return found == Operators.end() ? nullptr : &*found;
}

const BuiltinOperator* FindBuiltinOperator(std::string_view left, std::string_view operation, std::string_view right) {
    static const BuiltinOperator comparison{{}, {}, {}, "bool", {}};
    if ((operation == "==" || operation == "!=") && left == right) return &comparison;
    if ((operation == "<" || operation == ">" || operation == "<=" || operation == ">=") && ((left == "int" || left == "float") && (right == "int" || right == "float"))) return &comparison;
    const auto found = std::find_if(Operators.begin(), Operators.end(), [&](const auto& value) { return !value.right.empty() && value.left == left && value.operation == operation && value.right == right; });
    return found == Operators.end() ? nullptr : &*found;
}

const std::vector<BuiltinFunction>& BuiltinFunctions() { return Functions; }
const BuiltinFunction* FindBuiltinFunction(std::string_view name) {
    const auto found = std::find_if(Functions.begin(), Functions.end(), [&](const auto& value) { return value.name == name; });
    return found == Functions.end() ? nullptr : &*found;
}

std::optional<Lowering> BuiltinCastLowering(std::string_view target, std::string_view source) {
    if (target == source) return Lowering::CastIdentity;
    if (target == "string") return Lowering::CastString;
    if (source == "any" && (target == "int" || target == "float" || target == "bool")) return Lowering::CastDynamic;
    if (target == "bool") {
        if (source == "int" || source == "float") return Lowering::CastBoolNumber;
        if (source == "string") return Lowering::CastBoolString;
        if (source == "bool") return Lowering::CastIdentity;
    }
    if (target == "int") {
        if (source == "float") return Lowering::CastIntFloat;
        if (source == "bool") return Lowering::CastNumberBool;
        if (source == "string") return Lowering::CastNumberString;
    }
    if (target == "float") {
        if (source == "int") return Lowering::CastIdentity;
        if (source == "bool") return Lowering::CastNumberBool;
        if (source == "string") return Lowering::CastNumberString;
    }
    return std::nullopt;
}

} // namespace luasharp
