#include "Builtins.h"

#include <algorithm>
#include <map>
#include <mutex>

namespace luasharp {
namespace {

using P = BuiltinParameter;
using M = BuiltinMember;

const std::vector<std::string> NativeStructs{"Vec2", "Vec3", "Vec2i", "Vec3i", "Quat", "Mat4x4"};
const std::vector<std::string> StaticClasses{"math"};
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
    {"string", "is_empty", "bool", {}, SymbolKind::Method, false, Lowering::None, "ls_string_is_empty"},
    {"string", "index_of", "int", {{"string", "value"}, {"int", "start", true, "0"}}, SymbolKind::Method, false, Lowering::None, "ls_string_index_of"},
    {"string", "last_index_of", "int", {{"string", "value"}}, SymbolKind::Method, false, Lowering::None, "ls_string_last_index_of"},
    {"string", "count", "int", {{"string", "value"}}, SymbolKind::Method, false, Lowering::None, "ls_string_count"},
    {"string", "char_at", "string", {{"int", "index"}}, SymbolKind::Method, false, Lowering::None, "ls_string_char_at"},
    {"string", "byte_at", "int", {{"int", "index"}}, SymbolKind::Method, false, Lowering::None, "ls_string_byte_at"},
    {"string", "trim", "string", {}, SymbolKind::Method, false, Lowering::None, "ls_string_trim"},
    {"string", "trim_start", "string", {}, SymbolKind::Method, false, Lowering::None, "ls_string_trim_start"},
    {"string", "trim_end", "string", {}, SymbolKind::Method, false, Lowering::None, "ls_string_trim_end"},
    {"string", "replace", "string", {{"string", "from"}, {"string", "to"}}, SymbolKind::Method, false, Lowering::None, "ls_string_replace"},
    {"string", "repeat", "string", {{"int", "times"}}, SymbolKind::Method, false, Lowering::None, "ls_string_repeat"},
    {"string", "pad_left", "string", {{"int", "width"}, {"string", "fill", true, "ls_intern(\" \")"}}, SymbolKind::Method, false, Lowering::None, "ls_string_pad_left"},
    {"string", "pad_right", "string", {{"int", "width"}, {"string", "fill", true, "ls_intern(\" \")"}}, SymbolKind::Method, false, Lowering::None, "ls_string_pad_right"},
    {"string", "insert", "string", {{"int", "index"}, {"string", "value"}}, SymbolKind::Method, false, Lowering::None, "ls_string_insert"},
    {"string", "split", "string[]", {{"string", "separator"}}, SymbolKind::Method, false, Lowering::None, "ls_string_split"},
    {"string", "compare", "int", {{"string", "other"}}, SymbolKind::Method, false, Lowering::None, "ls_string_compare"},
    {"string", "equals_ignore_case", "bool", {{"string", "other"}}, SymbolKind::Method, false, Lowering::None, "ls_string_equals_ignore_case"},


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
const std::vector<std::string>& BuiltinStaticClasses() { return StaticClasses; }
bool IsBuiltinStaticClass(std::string_view type) { return std::find(StaticClasses.begin(), StaticClasses.end(), type) != StaticClasses.end(); }

namespace {
// Members of a list type, created once per list type. Never erased, so returned pointers stay valid.
const std::vector<BuiltinMember>& ListMembers(std::string_view type) {
    static std::mutex mutex;
    static std::map<std::string, std::vector<BuiltinMember>, std::less<>> lists;
    std::lock_guard lock(mutex);
    auto found = lists.find(type);
    if (found != lists.end()) return found->second;
    const auto element = ElementType(type);
    const std::string owner(type);
    const auto native = [&](const char* name, std::string result, std::vector<BuiltinParameter> parameters, std::string cName, Lowering lowering = Lowering::None) {
        return M{owner, name, std::move(result), std::move(parameters), SymbolKind::Method, false, lowering, std::move(cName)};
    };
    const bool numeric = element == "int" || element == "float";
    const bool ordered = numeric || element == "string";
    std::vector<M> members{
        {owner, "push_back", "void", {{element, "value"}}, SymbolKind::Method, false, Lowering::ListPushBack},
        {owner, "pop_back", element, {}, SymbolKind::Method, false, Lowering::ListPopBack},
        {owner, "size", "int", {}, SymbolKind::Method, false, Lowering::ListSize},
        {owner, "empty", "bool", {}, SymbolKind::Method, false, Lowering::ListEmpty},
        {owner, "clear", "void", {}, SymbolKind::Method, false, Lowering::ListClear},
        {owner, "first", element, {}, SymbolKind::Method, false, Lowering::ListFirst},
        {owner, "last", element, {}, SymbolKind::Method, false, Lowering::ListLast},
        native("insert", "void", {{"int", "index"}, {element, "value"}}, "ls_list_insert", Lowering::ListElementArgument),
        native("remove_at", "void", {{"int", "index"}}, "ls_list_remove_at"),
        native("reverse", "void", {}, "ls_list_reverse"),
        native("extend", "void", {{owner, "other"}}, "ls_list_extend"),
        native("copy", owner, {}, "ls_list_copy"),
    };
    // Equality compares number values and reference identity; native math structs are not comparable this way.
    if (!IsNativeStruct(element)) {
        members.push_back(native("index_of", "int", {{element, "value"}}, "ls_list_index_of", Lowering::ListElementArgument));
        members.push_back(native("contains", "bool", {{element, "value"}}, "ls_list_contains", Lowering::ListElementArgument));
        members.push_back(native("remove", "bool", {{element, "value"}}, "ls_list_remove", Lowering::ListElementArgument));
    }
    if (ordered) members.push_back(native("sort", "void", {}, "ls_list_sort"));
    if (numeric) for (const char* name : {"sum", "min", "max"}) members.push_back(native(name, element, {}, std::string("ls_list_") + name + "_" + element));
    if (ordered || element == "bool") members.push_back(native("join", "string", {{"string", "separator", true, "ls_intern(\"\")"}}, "ls_list_join"));
    return lists.emplace(owner, std::move(members)).first->second;
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
