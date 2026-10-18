#pragma once

#include "CBackend.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace luasharp {

// What a hot reload from one module to its replacement does (PlanReload).
struct NativeReloadPlan {
    std::vector<std::string> errors;   // the reload cannot be done; the old module stays the program
    std::vector<std::string> warnings; // values that are reset
    std::unordered_map<const void*, const void*> types; // LS_TypeInfo of each kept class and struct: old -> new
};

struct NativeReloadResult {
    size_t retypedObjects{};
    size_t updatedDefaults{}; // values that had the old constant of their initializer and now have the new one
};

// Compares the layouts and finds the objects of removed classes. Changes nothing.
// next must use the runtime of current and must not be initialized.
[[nodiscard]] NativeReloadPlan PlanReload(const NativeModule& current, const NativeModule& next);

// Needs a plan without errors. Initializes the static fields of next (not its top-level code), and moves the
// top-level variables and static fields of current into next, by name. A value that equals the old constant of its
// initializer gets the new constant, also in instance fields. Then the objects of current's classes get the types of
// next. current must stay loaded: closures that it made still run its code.
NativeReloadResult ApplyReload(const NativeModule& current, NativeModule& next, const NativeReloadPlan& plan);

} // namespace luasharp
