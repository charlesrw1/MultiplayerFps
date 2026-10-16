#include "Builtins.h"

#include <algorithm>

namespace luasharp {
namespace {

using P = BuiltinParameter;
using M = BuiltinMember;

const std::vector<std::string> Types{"int", "float", "bool", "string", "table", "Vec3", "Quat", "Transform"};

const std::vector<M> Members{
    {"string", "length", "int", {}, SymbolKind::Method, false, "string_length"},
    {"string", "size", "int", {}, SymbolKind::Method, false, "string_length"},
    {"string", "contains", "bool", {{"string", "value"}}, SymbolKind::Method, false, "string_contains"},
    {"string", "starts_with", "bool", {{"string", "prefix"}}, SymbolKind::Method, false, "string_starts_with"},
    {"string", "ends_with", "bool", {{"string", "suffix"}}, SymbolKind::Method, false, "string_ends_with"},
    {"string", "substring", "string", {{"int", "start"}, {"int", "length", true}}, SymbolKind::Method, false, "string_substring"},
    {"string", "lower", "string", {}, SymbolKind::Method, false, "string_lower"},
    {"string", "upper", "string", {}, SymbolKind::Method, false, "string_upper"},

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
    {"string", "+", "string", "string", "concat"},
    {"Vec3", "+", "Vec3", "Vec3", {}}, {"Vec3", "-", "Vec3", "Vec3", {}}, {"Vec3", "*", "Vec3", "Vec3", {}}, {"Vec3", "*", "float", "Vec3", {}}, {"Vec3", "*", "int", "Vec3", {}},
    {"Quat", "*", "Quat", "Quat", {}}, {"Quat", "*", "Vec3", "Vec3", {}},
    {"Transform", "*", "Transform", "Transform", {}}, {"Transform", "*", "Vec3", "Vec3", {}},
};

std::string ElementType(std::string_view array) { return std::string(array.substr(0, array.size() - 2)); }

} // namespace

const std::vector<std::string>& BuiltinTypes() { return Types; }
bool IsBuiltinType(std::string_view type) { return std::find(Types.begin(), Types.end(), type) != Types.end(); }

std::vector<BuiltinMember> BuiltinMembers(std::string_view type, bool staticMembers) {
    if (type.ends_with("[]") && !staticMembers) {
        const auto element = ElementType(type);
        return {
            {std::string(type), "push_back", "void", {{element, "value"}}, SymbolKind::Method, false, "list_push_back"},
            {std::string(type), "pop_back", element, {}, SymbolKind::Method, false, "list_pop_back"},
            {std::string(type), "size", "int", {}, SymbolKind::Method, false, "list_size"},
            {std::string(type), "empty", "bool", {}, SymbolKind::Method, false, "list_empty"},
            {std::string(type), "clear", "void", {}, SymbolKind::Method, false, "list_clear"},
        };
    }
    std::vector<BuiltinMember> result;
    for (const auto& member : Members) if (member.owner == type && member.isStatic == staticMembers) result.push_back(member);
    return result;
}

std::optional<BuiltinMember> FindBuiltinMember(std::string_view type, std::string_view name, bool staticMember) {
    auto members = BuiltinMembers(type, staticMember);
    const auto found = std::find_if(members.begin(), members.end(), [&](const auto& value) { return value.name == name; });
    return found == members.end() ? std::nullopt : std::optional<BuiltinMember>(*found);
}

std::optional<BuiltinOperator> FindBuiltinOperator(std::string_view left, std::string_view operation, std::string_view right) {
    if ((operation == "==" || operation == "!=") && left == right) return BuiltinOperator{std::string(left), std::string(operation), std::string(right), "bool", {}};
    if ((operation == "<" || operation == ">" || operation == "<=" || operation == ">=") && ((left == "int" || left == "float") && (right == "int" || right == "float"))) return BuiltinOperator{std::string(left), std::string(operation), std::string(right), "bool", {}};
    const auto found = std::find_if(Operators.begin(), Operators.end(), [&](const auto& value) { return value.left == left && value.operation == operation && value.right == right; });
    return found == Operators.end() ? std::nullopt : std::optional<BuiltinOperator>(*found);
}

std::optional<std::string> BuiltinCastLowering(std::string_view target, std::string_view source) {
    if (target == source) return "cast_identity";
    if (target == "string") return "cast_string";
    if (target == "bool") {
        if (source == "int" || source == "float") return "cast_bool_number";
        if (source == "string") return "cast_bool_string";
        if (source == "bool") return "cast_identity";
    }
    if (target == "int") {
        if (source == "float") return "cast_int_float";
        if (source == "bool") return "cast_number_bool";
        if (source == "string") return "cast_number_string";
    }
    if (target == "float") {
        if (source == "int") return "cast_identity";
        if (source == "bool") return "cast_number_bool";
        if (source == "string") return "cast_number_string";
    }
    return std::nullopt;
}

} // namespace luasharp
