#include "NativeReload.h"

#include "NativeAbi.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <string_view>

namespace luasharp {
namespace {
using namespace abi;

// A value of a constant initializer (NativeConstantSymbol). Mat4x4 is the largest.
struct Constant {
    alignas(16) std::array<std::byte, 256> bytes{};
    size_t size{};
};

Constant ReadConstant(const NativeModule& module, const NativeConstantSymbol& symbol) {
    const auto write = reinterpret_cast<unsigned long long (*)(void*)>(module.FindSymbol(symbol));
    assert(write && "the C backend emits each constant it lists");
    Constant constant;
    constant.size = static_cast<size_t>(write(constant.bytes.data()));
    assert(constant.size <= constant.bytes.size());
    return constant;
}
std::string_view Text(const void* string) {
    if (!string) return {};
    const auto* value = static_cast<const RuntimeString*>(string);
    return {value->data, static_cast<size_t>(value->length)};
}
// Strings compare by text. The other types of constants have no identity.
bool Equal(const std::byte* slot, const Constant& constant, bool isString) {
    if (isString) return Text(Load<void*>(slot)) == Text(Load<void*>(constant.bytes.data()));
    return std::memcmp(slot, constant.bytes.data(), constant.size) == 0;
}
void Store(std::byte* slot, const Constant& constant) { std::memcpy(slot, constant.bytes.data(), constant.size); }

template <typename T> const T* FindByName(const std::vector<T>& values, std::string_view name) {
    const auto found = std::ranges::find(values, name, &T::name);
    return found == values.end() ? nullptr : &*found;
}
bool IsTopLevel(const NativeGlobal& global) { return global.name.find('.') == std::string::npos; }
bool IsString(std::string_view type) { return type == "string"; }

void* Symbol(const NativeModule& module, const std::string& name) {
    void* symbol = module.FindSymbol(name);
    assert(symbol && "the C backend emits the helpers of each global (NativeGlobal)");
    return symbol;
}

void MoveGlobal(const NativeModule& current, const NativeModule& next, const NativeGlobal& global, NativeReloadResult& result) {
    auto* slot = static_cast<std::byte*>(next.FindGlobal(global.name));
    assert(slot);
    const auto* old = FindByName(current.Generated().globals, global.name);
    if (!old || old->type != global.type) {
        if (!IsTopLevel(global)) return; // InitializeStatics gave it its value
        if (!global.constant.empty()) Store(slot, ReadConstant(next, global.constant));
        else reinterpret_cast<void (*)()>(Symbol(next, global.symbolName + "__reset"))();
        return;
    }
    const auto* from = static_cast<const std::byte*>(current.FindGlobal(old->name));
    assert(from);
    if (!old->constant.empty() && !global.constant.empty()) {
        const auto before = ReadConstant(current, old->constant), after = ReadConstant(next, global.constant);
        if (Equal(from, before, IsString(global.type)) && !Equal(before.bytes.data(), after, IsString(global.type))) {
            Store(slot, after);
            ++result.updatedDefaults;
            return;
        }
    }
    const auto size = reinterpret_cast<unsigned long long (*)()>(Symbol(next, global.symbolName + "__size"))();
    std::memcpy(slot, from, static_cast<size_t>(size));
}

size_t FieldOffset(const NativeTypeInfo& type, std::string_view field) {
    for (const auto& info : Fields(type))
        if (info.name == field) return static_cast<size_t>(info.offset);
    assert(!"a field with a constant has a kind, so the type info lists it");
    return 0;
}

// An instance field whose constant changed, in the class that declares it.
struct FieldChange {
    std::string_view field;
    Constant before, after;
    bool isString{};
};

void UpdateFieldDefaults(const NativeModule& current, const NativeModule& next, NativeReloadResult& result) {
    std::unordered_map<std::string_view, std::vector<FieldChange>> changes; // by class
    for (const auto& layout : next.Generated().types) {
        const auto* old = layout.isStruct ? nullptr : FindByName(current.Generated().types, layout.name);
        if (!old) continue;
        assert(*old == layout); // PlanReload
        for (size_t i = 0; i < layout.fields.size(); ++i) {
            const auto& field = layout.fields[i];
            if (field.constant.empty() || old->fields[i].constant.empty()) continue;
            FieldChange change{field.name, ReadConstant(current, old->fields[i].constant), ReadConstant(next, field.constant), IsString(field.type)};
            if (!Equal(change.before.bytes.data(), change.after, change.isString)) changes[layout.name].push_back(std::move(change));
        }
    }
    if (changes.empty()) return;
    const auto& runtime = *next.Runtime();
    for (const auto& layout : next.Generated().types) {
        if (layout.isStruct) continue;
        const auto& type = *static_cast<const NativeTypeInfo*>(next.ScriptType(layout.name));
        std::vector<const FieldChange*> applied; // of the class and its bases
        for (const auto* t = &type; t; t = t->base)
            if (const auto found = changes.find(t->name); found != changes.end())
                for (const auto& change : found->second) applied.push_back(&change);
        if (applied.empty()) continue;
        const auto objects = runtime.ObjectsOf(&type);
        for (const auto* change : applied) {
            const size_t offset = FieldOffset(type, change->field);
            for (void* object : objects) {
                auto* slot = static_cast<std::byte*>(object) + offset;
                if (!Equal(slot, change->before, change->isString)) continue;
                Store(slot, change->after);
                ++result.updatedDefaults;
            }
        }
    }
}
} // namespace

NativeReloadPlan PlanReload(const NativeModule& current, const NativeModule& next) {
    assert(current.Runtime() && current.Runtime() == next.Runtime());
    NativeReloadPlan plan;
    const auto& runtime = *current.Runtime();
    for (const auto& layout : current.Generated().types) {
        const auto* replacement = FindByName(next.Generated().types, layout.name);
        const std::string what = (layout.isStruct ? "struct '" : "class '") + layout.name + "'";
        if (!replacement) {
            if (layout.isStruct) continue; // boxes of it keep the old type info
            if (const auto count = runtime.ObjectsOf(current.ScriptType(layout.name)).size())
                plan.errors.push_back(what + " is removed, but " + std::to_string(count) + " of its objects are alive");
            continue;
        }
        if (*replacement != layout) {
            plan.errors.push_back(what + " changed its fields or its base; a restart applies this change");
            continue;
        }
        plan.types.emplace(current.ScriptType(layout.name), next.ScriptType(layout.name));
    }
    for (const auto& global : next.Generated().globals) {
        const auto* old = FindByName(current.Generated().globals, global.name);
        if (old && old->type != global.type)
            plan.warnings.push_back("'" + global.name + "' changed its type from " + old->type + " to " + global.type + ", so it gets its initial value");
        else if (!old && IsTopLevel(global) && global.constant.empty())
            plan.warnings.push_back("new top-level variable '" + global.name + "' gets the default value of its type; its initializer runs at the next start");
    }
    return plan;
}

NativeReloadResult ApplyReload(const NativeModule& current, NativeModule& next, const NativeReloadPlan& plan) {
    assert(plan.errors.empty());
    NativeReloadResult result;
    next.InitializeStatics();
    for (const auto& global : next.Generated().globals) MoveGlobal(current, next, global, result);
    result.retypedObjects = next.Runtime()->Retype(plan.types);
    UpdateFieldDefaults(current, next, result);
    return result;
}

} // namespace luasharp
