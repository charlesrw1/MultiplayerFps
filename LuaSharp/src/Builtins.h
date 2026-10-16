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
    Lowering lowering{};
};

struct BuiltinOperator {
    std::string left;
    std::string operation;
    std::string right;
    std::string result;
    Lowering lowering{};
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
std::vector<BuiltinMember> BuiltinMembers(std::string_view type, bool staticMembers = false);
// Lookups return pointers into static tables that live for the whole program.
const BuiltinMember* FindBuiltinMember(std::string_view type, std::string_view name, bool staticMember = false);
const BuiltinOperator* FindBuiltinOperator(std::string_view left, std::string_view operation, std::string_view right);
const std::vector<BuiltinFunction>& BuiltinFunctions();
const BuiltinFunction* FindBuiltinFunction(std::string_view name);
std::optional<Lowering> BuiltinCastLowering(std::string_view target, std::string_view source);
std::optional<std::string> BuiltinDefaultValue(std::string_view type);

} // namespace luasharp
