#pragma once

#include "Semantic.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace luasharp {

struct BuiltinParameter {
    std::string type;
    std::string name;
    bool optional{};
    std::string defaultValue; // C expression emitted for an omitted optional argument
};

struct BuiltinMember {
    std::string owner;
    std::string name;
    std::string returnType;
    std::vector<BuiltinParameter> parameters;
    SymbolKind kind{SymbolKind::Method};
    bool isStatic{};
    Lowering lowering{};
    std::string cName; // native C function (instance methods take the receiver first); a static Field's cName is its C expression; empty for fields
};

struct BuiltinOperator {
    std::string left;
    std::string operation;
    std::string right;
    std::string result;
    Lowering lowering{};
    std::string cName; // native C function taking the operands; empty when the backend emits the operator itself
};

struct BuiltinFunction {
    std::string name;
    std::string returnType;
    std::vector<BuiltinParameter> parameters;
    bool variadic{};
    Lowering lowering{};
};

const std::vector<std::string>& BuiltinTypes();
bool IsBuiltinType(std::string_view type);
// Native value types (Vec2, Vec3, Vec2i, Vec3i, Quat, Transform): C structs passed and stored by value.
bool IsNativeStruct(std::string_view type);
const std::vector<std::string>& NativeStructTypes();
// Static-only classes such as `math`; they are not types of values.
bool IsBuiltinStaticClass(std::string_view type);
std::vector<const BuiltinMember*> BuiltinMembers(std::string_view type, bool staticMembers = false);
// Lookups return pointers into static tables that live for the whole program.
const BuiltinMember* FindBuiltinMember(std::string_view type, std::string_view name, bool staticMember = false);
// Every overload of a member, in declaration order.
std::vector<const BuiltinMember*> FindBuiltinMembers(std::string_view type, std::string_view name, bool staticMember = false);
const BuiltinOperator* FindBuiltinOperator(std::string_view left, std::string_view operation, std::string_view right);
const BuiltinOperator* FindBuiltinUnary(std::string_view operand, std::string_view operation);
const std::vector<BuiltinFunction>& BuiltinFunctions();
const BuiltinFunction* FindBuiltinFunction(std::string_view name);
std::optional<Lowering> BuiltinCastLowering(std::string_view target, std::string_view source);

} // namespace luasharp
