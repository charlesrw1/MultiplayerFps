#include "Builtins.h"

#include <algorithm>
#include <map>
#include <mutex>

namespace luasharp {
namespace {

using P = BuiltinParameter;
using M = BuiltinMember;

const std::vector<std::string> Types{"int", "float", "bool", "string", "table", "Vec3", "Quat", "Transform"};

const std::vector<M> Members{
    {"string", "length", "int", {}, SymbolKind::Method, false, Lowering::StringLength},
    {"string", "size", "int", {}, SymbolKind::Method, false, Lowering::StringLength},
    {"string", "contains", "bool", {{"string", "value"}}, SymbolKind::Method, false, Lowering::StringContains},
    {"string", "starts_with", "bool", {{"string", "prefix"}}, SymbolKind::Method, false, Lowering::StringStartsWith},
    {"string", "ends_with", "bool", {{"string", "suffix"}}, SymbolKind::Method, false, Lowering::StringEndsWith},
    {"string", "substring", "string", {{"int", "start"}, {"int", "length", true}}, SymbolKind::Method, false, Lowering::StringSubstring},
    {"string", "lower", "string", {}, SymbolKind::Method, false, Lowering::StringLower},
    {"string", "upper", "string", {}, SymbolKind::Method, false, Lowering::StringUpper},

    {"Vec3", "x", "float", {}, SymbolKind::Field}, {"Vec3", "y", "float", {}, SymbolKind::Field}, {"Vec3", "z", "float", {}, SymbolKind::Field},
    {"Vec3", "length", "float", {}, SymbolKind::Method},
    {"Vec3", "normalize", "Vec3", {}, SymbolKind::Method},
    {"Vec3", "dot", "float", {{"Vec3", "other"}}, SymbolKind::Method},
    {"Vec3", "cross", "Vec3", {{"Vec3", "other"}}, SymbolKind::Method},
    {"Vec3", "clamp", "Vec3", {{"Vec3", "min"}, {"Vec3", "max"}}, SymbolKind::Method},
    {"Vec3", "min", "Vec3", {{"Vec3", "other"}}, SymbolKind::Method},
    {"Vec3", "max", "Vec3", {{"Vec3", "other"}}, SymbolKind::Method},
    {"Vec3", "mix", "Vec3", {{"Vec3", "other"}, {"float", "t"}}, SymbolKind::Method},
    {"Vec3", "new", "Vec3", {{"float", "x", true}, {"float", "y", true}, {"float", "z", true}}, SymbolKind::Method, true},
    {"Vec3", "splat", "Vec3", {{"float", "value"}}, SymbolKind::Method, true},

    {"Quat", "w", "float", {}, SymbolKind::Field}, {"Quat", "x", "float", {}, SymbolKind::Field}, {"Quat", "y", "float", {}, SymbolKind::Field}, {"Quat", "z", "float", {}, SymbolKind::Field},
    {"Quat", "inverse", "Quat", {}, SymbolKind::Method},
    {"Quat", "to_euler", "Vec3", {}, SymbolKind::Method},
    {"Quat", "slerp", "Quat", {{"Quat", "other"}, {"float", "alpha"}}, SymbolKind::Method},
    {"Quat", "delta_to", "Quat", {{"Quat", "other"}}, SymbolKind::Method},
    {"Quat", "new", "Quat", {{"float", "w", true}, {"float", "x", true}, {"float", "y", true}, {"float", "z", true}}, SymbolKind::Method, true},
    {"Quat", "identity", "Quat", {}, SymbolKind::Method, true},
    {"Quat", "from_euler", "Quat", {{"Vec3", "euler"}}, SymbolKind::Method, true},

    {"Transform", "inverse", "Transform", {}, SymbolKind::Method},
    {"Transform", "translation", "Vec3", {}, SymbolKind::Method},
    {"Transform", "rotation", "Quat", {}, SymbolKind::Method},
    {"Transform", "scale", "Vec3", {}, SymbolKind::Method},
    {"Transform", "decompose", "table", {}, SymbolKind::Method},
    {"Transform", "set_translation", "Transform", {{"Vec3", "position"}}, SymbolKind::Method},
    {"Transform", "transform_point", "Vec3", {{"Vec3", "point"}}, SymbolKind::Method},
    {"Transform", "transform_direction", "Vec3", {{"Vec3", "direction"}}, SymbolKind::Method},
    {"Transform", "mul", "Transform", {{"Transform", "other"}}, SymbolKind::Method},
    {"Transform", "new", "Transform", {}, SymbolKind::Method, true},
    {"Transform", "identity", "Transform", {}, SymbolKind::Method, true},
    {"Transform", "from_pos", "Transform", {{"Vec3", "position"}}, SymbolKind::Method, true},
    {"Transform", "from_pos_rot", "Transform", {{"Vec3", "position"}, {"Quat", "rotation"}}, SymbolKind::Method, true},
    {"Transform", "from_pos_rot_scale", "Transform", {{"Vec3", "position"}, {"Quat", "rotation"}, {"Vec3", "scale"}}, SymbolKind::Method, true},
    {"Transform", "look_at", "Transform", {{"Vec3", "eye"}, {"Vec3", "target"}, {"Vec3", "up"}}, SymbolKind::Method, true},
};

const std::vector<BuiltinOperator> Operators{
    {"int", "+", "int", "int", {}}, {"int", "-", "int", "int", {}}, {"int", "*", "int", "int", {}}, {"int", "/", "int", "float", {}}, {"int", "%", "int", "int", {}},
    {"float", "+", "float", "float", {}}, {"float", "-", "float", "float", {}}, {"float", "*", "float", "float", {}}, {"float", "/", "float", "float", {}},
    {"float", "+", "int", "float", {}}, {"float", "-", "int", "float", {}}, {"float", "*", "int", "float", {}}, {"float", "/", "int", "float", {}},
    {"int", "+", "float", "float", {}}, {"int", "-", "float", "float", {}}, {"int", "*", "float", "float", {}}, {"int", "/", "float", "float", {}},
    {"string", "+", "string", "string", Lowering::Concat},
    {"Vec3", "+", "Vec3", "Vec3", {}}, {"Vec3", "-", "Vec3", "Vec3", {}}, {"Vec3", "*", "Vec3", "Vec3", {}}, {"Vec3", "*", "float", "Vec3", {}}, {"Vec3", "*", "int", "Vec3", {}},
    {"Quat", "*", "Quat", "Quat", {}}, {"Quat", "*", "Vec3", "Vec3", {}},
    {"Transform", "*", "Transform", "Transform", {}}, {"Transform", "*", "Vec3", "Vec3", {}},
};

const std::vector<BuiltinFunction> Functions{
    {"printf", "void", {{"string", "format"}}, true, Lowering::Printf},
};

std::string ElementType(std::string_view array) { return std::string(array.substr(0, array.size() - 2)); }

} // namespace

const std::vector<std::string>& BuiltinTypes() { return Types; }
bool IsBuiltinType(std::string_view type) { return std::find(Types.begin(), Types.end(), type) != Types.end(); }

std::vector<BuiltinMember> BuiltinMembers(std::string_view type, bool staticMembers) {
    if (type.ends_with("[]") && !staticMembers) {
        const auto element = ElementType(type);
        return {
            {std::string(type), "push_back", "void", {{element, "value"}}, SymbolKind::Method, false, Lowering::ListPushBack},
            {std::string(type), "pop_back", element, {}, SymbolKind::Method, false, Lowering::ListPopBack},
            {std::string(type), "size", "int", {}, SymbolKind::Method, false, Lowering::ListSize},
            {std::string(type), "empty", "bool", {}, SymbolKind::Method, false, Lowering::ListEmpty},
            {std::string(type), "clear", "void", {}, SymbolKind::Method, false, Lowering::ListClear},
        };
    }
    std::vector<BuiltinMember> result;
    for (const auto& member : Members) if (member.owner == type && member.isStatic == staticMembers) result.push_back(member);
    return result;
}

const BuiltinMember* FindBuiltinMember(std::string_view type, std::string_view name, bool staticMember) {
    const std::vector<BuiltinMember>* members = &Members;
    if (type.ends_with("[]")) {
        if (staticMember) return nullptr;
        static std::mutex mutex;
        static std::map<std::string, std::vector<BuiltinMember>, std::less<>> lists; // never erased, so returned pointers stay valid
        std::lock_guard lock(mutex);
        auto found = lists.find(type);
        if (found == lists.end()) found = lists.emplace(std::string(type), BuiltinMembers(type, false)).first;
        members = &found->second;
    }
    const auto found = std::find_if(members->begin(), members->end(), [&](const auto& value) { return value.owner == type && value.isStatic == staticMember && value.name == name; });
    return found == members->end() ? nullptr : &*found;
}

const BuiltinOperator* FindBuiltinOperator(std::string_view left, std::string_view operation, std::string_view right) {
    static const BuiltinOperator comparison{{}, {}, {}, "bool", {}};
    if ((operation == "==" || operation == "!=") && left == right) return &comparison;
    if ((operation == "<" || operation == ">" || operation == "<=" || operation == ">=") && ((left == "int" || left == "float") && (right == "int" || right == "float"))) return &comparison;
    const auto found = std::find_if(Operators.begin(), Operators.end(), [&](const auto& value) { return value.left == left && value.operation == operation && value.right == right; });
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

std::optional<std::string> BuiltinDefaultValue(std::string_view type) {
    if (type.ends_with("[]")) return "{}";
    if (type == "int") return "0";
    if (type == "float") return "0.0";
    if (type == "bool") return "false";
    if (type == "string") return "\"\"";
    if (type == "table") return "{}";
    if (type == "Vec3") return "Vec3.new()";
    if (type == "Quat") return "Quat.identity()";
    if (type == "Transform") return "Transform.identity()";
    return std::nullopt;
}

} // namespace luasharp
