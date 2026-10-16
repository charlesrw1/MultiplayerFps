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
};

struct BuiltinMember {
    std::string owner;
    std::string name;
    std::string returnType;
    std::vector<BuiltinParameter> parameters;
    SymbolKind kind{SymbolKind::Method};
    bool isStatic{};
    std::string lowering;
};

struct BuiltinOperator {
    std::string left;
    std::string operation;
    std::string right;
    std::string result;
    std::string lowering;
};

struct BuiltinFunction {
    std::string name;
    std::string returnType;
    std::vector<BuiltinParameter> parameters;
    bool variadic{};
    std::string lowering;
};

const std::vector<std::string>& BuiltinTypes();
bool IsBuiltinType(std::string_view type);
std::vector<BuiltinMember> BuiltinMembers(std::string_view type, bool staticMembers = false);
std::optional<BuiltinMember> FindBuiltinMember(std::string_view type, std::string_view name, bool staticMember = false);
std::optional<BuiltinOperator> FindBuiltinOperator(std::string_view left, std::string_view operation, std::string_view right);
const std::vector<BuiltinFunction>& BuiltinFunctions();
std::optional<BuiltinFunction> FindBuiltinFunction(std::string_view name);
std::optional<std::string> BuiltinCastLowering(std::string_view target, std::string_view source);
std::optional<std::string> BuiltinDefaultValue(std::string_view type);

} // namespace luasharp
