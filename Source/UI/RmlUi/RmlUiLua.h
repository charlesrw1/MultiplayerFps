#pragma once
// Lua-only binding surface for RmlUi (documents, data models, element
// properties, events). Hand-written lua_CFunctions registered onto a global
// "RmlUi" table, following the same pattern as register_lua_transform /
// register_lua_vec_quat (Source/Scripting/LuaTransform.h) - the ClassBase/
// REF codegen path only marshals a fixed set of known C++ types, and data
// model values are dynamically-typed Lua values, so this bypasses codegen
// rather than fighting it.
extern "C" {
#include <lua.h>
}

// Registers the global "RmlUi" table into L. Call once per lua_State
// alongside register_lua_transform/register_lua_vec_quat.
void register_rmlui_lua(lua_State* L);

// Drops all data models and event listener refs. Call from
// RmlUiSystem::shutdown() before Rml::Shutdown() destroys the contexts
// these reference.
void rmlui_lua_reset();
