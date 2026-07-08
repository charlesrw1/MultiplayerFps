#include "ScriptManager.h"
#include "Framework/InterfaceTypeInfo.h"
#include "Framework/StringUtils.h"
#include "Framework/MapUtil.h"
#include "Game/EntityComponent.h"
#include "Game/LevelAssets.h"
#include "GameEnginePublic.h"
#include <cassert>
#include <cstring>
#include <new>
#include <string>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// Snapshot of one PROP_LUA_BACKED field's value, used to preserve overlapping fields
// (matched by name + core_type_id) across a Lua class reload.
struct LuaFieldSnapshot
{
	core_type_id type;
	float       f = 0;
	int32_t     i = 0;
	int8_t      b = 0;
	std::string s;
};

struct LuaInstanceSnapshot
{
	Component* inst = nullptr;
	std::unordered_map<std::string, LuaFieldSnapshot> values;
};

static LuaFieldSnapshot snapshot_field(const PropertyInfo& pi, uint8_t* p) {
	LuaFieldSnapshot v;
	v.type = pi.type;
	switch (pi.type) {
	case core_type_id::Float: v.f = *(float*)p; break;
	case core_type_id::Int32:
	case core_type_id::Enum32: v.i = *(int32_t*)p; break;
	case core_type_id::Bool: v.b = *(int8_t*)p; break;
	case core_type_id::StdString: v.s = *(std::string*)p; break;
	default: break;
	}
	return v;
}

static void restore_field(const LuaFieldSnapshot& v, const PropertyInfo& pi, uint8_t* p) {
	if (v.type != pi.type)
		return; // type changed across reload — fall through to template default
	switch (pi.type) {
	case core_type_id::Float: *(float*)p = v.f; break;
	case core_type_id::Int32:
	case core_type_id::Enum32: *(int32_t*)p = v.i; break;
	case core_type_id::Bool: *(int8_t*)p = v.b; break;
	case core_type_id::StdString: *(std::string*)p = v.s; break;
	default: break;
	}
}

// Maps a Lua ---@type annotation string to a (core_type_id, optional EnumTypeInfo*, optional
// custom_type_str) tuple. Returns true on a recognized type. Unrecognized types (vec3, tables,
// asset refs, etc.) are deferred to a later feature pass.
static bool lua_type_str_to_core_type(const string& type_str, core_type_id& out_type,
									  const EnumTypeInfo*& out_enum, const char*& out_custom_type) {
	out_enum = nullptr;
	out_custom_type = "";
	if (type_str == "number" || type_str == "float") {
		out_type = core_type_id::Float;
		return true;
	}
	if (type_str == "integer" || type_str == "int") {
		out_type = core_type_id::Int32;
		return true;
	}
	if (type_str == "boolean" || type_str == "bool") {
		out_type = core_type_id::Bool;
		return true;
	}
	if (type_str == "string") {
		out_type = core_type_id::StdString;
		return true;
	}
	if (type_str == "EntityTarget") {
		out_type = core_type_id::StdString;
		out_custom_type = "EntityTarget";
		return true;
	}
	if (auto e = EnumRegistry::find_enum_type(type_str)) {
		out_type = core_type_id::Enum32;
		out_enum = e;
		return true;
	}
	return false;
}

// Returns the byte size required to store a value of the given core_type_id in the
// per-instance shadow buffer. std::string is constructed/destructed via placement new
// when the buffer is (re)allocated.
static uint32_t lua_backed_size_for_type(core_type_id t) {
	switch (t) {
	case core_type_id::Bool: return 1;
	case core_type_id::Int32:
	case core_type_id::Enum32:
	case core_type_id::Float: return 4;
	case core_type_id::StdString: return sizeof(std::string);
	default: ASSERT(0); return 0;
	}
}

// Returns 4-byte alignment for primitives, alignof(std::string) for strings.
static uint32_t lua_backed_align_for_type(core_type_id t) {
	if (t == core_type_id::StdString) return alignof(std::string);
	if (t == core_type_id::Bool) return 1;
	return 4;
}

static uint32_t align_up(uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

static void synthesize_layout_from_parsed(LuaClassTypeInfo* /*self*/,
										  vector<PropertyInfo>& lua_props_storage,
										  PropertyInfoList& lua_props_list,
										  uint32_t& lua_field_shadow_size,
										  const string& lua_classname,
										  const vector<ParseProperty>& parsed_properties) {
	lua_props_storage.clear();
	lua_props_list = {};
	lua_field_shadow_size = 0;
	uint32_t cursor = 0;
	for (auto& parsed : parsed_properties) {
		core_type_id ctype;
		const EnumTypeInfo* einfo = nullptr;
		const char* custom_type = "";
		if (!lua_type_str_to_core_type(parsed.type_str, ctype, einfo, custom_type)) {
			sys_print(Warning, "LuaClassTypeInfo[%s]: skipping field '%s' with unsupported ---@type '%s'\n",
					  lua_classname.c_str(), parsed.name.c_str(), parsed.type_str.c_str());
			continue;
		}
		const uint32_t align = lua_backed_align_for_type(ctype);
		const uint32_t size  = lua_backed_size_for_type(ctype);
		cursor = align_up(cursor, align);

		PropertyInfo pi;
		// PropertyInfo holds a raw `const char*` to the name; the parsed_properties vector
		// owns the storage for the string for as long as this class exists.
		pi.name = parsed.name.c_str();
		pi.offset = (uint16_t)cursor;
		pi.flags = PROP_DEFAULT | PROP_LUA_BACKED;
		pi.type = ctype;
		pi.enum_type = einfo;
		pi.custom_type_str = custom_type;
		lua_props_storage.push_back(pi);
		cursor += size;
	}
	if (!lua_props_storage.empty()) {
		lua_props_list.list = lua_props_storage.data();
		lua_props_list.count = (int)lua_props_storage.size();
		lua_props_list.type_name = lua_classname.c_str();
		lua_field_shadow_size = cursor;
	}
}

// Placement-construct one PROP_LUA_BACKED field within a freshly-allocated shadow buffer.
// Primitives are left zero-initialized by the caller's memset; only non-POD types need work.
static void construct_lua_field(const PropertyInfo& pi, uint8_t* p) {
	if (pi.type == core_type_id::StdString)
		new (p) std::string();
}

// Inverse of construct_lua_field. Called before freeing the shadow buffer or
// reallocating it for a new layout.
static void destruct_lua_field(const PropertyInfo& pi, uint8_t* p) {
	if (pi.type == core_type_id::StdString)
		((std::string*)p)->~basic_string();
}

// Reads a default value for `pi` from the class's template Lua table (top of stack at
// `template_idx`) and writes it into `p` (already constructed). Missing/wrong-type
// template entries leave `p` at its constructed default.
static void apply_template_default(lua_State* L, int template_idx, const PropertyInfo& pi, uint8_t* p) {
	lua_getfield(L, template_idx, pi.name);
	if (!lua_isnil(L, -1)) {
		switch (pi.type) {
		case core_type_id::Float:
			*(float*)p = (float)lua_tonumber(L, -1);
			break;
		case core_type_id::Int32:
		case core_type_id::Enum32:
			*(int32_t*)p = (int32_t)lua_tointeger(L, -1);
			break;
		case core_type_id::Bool:
			*(int8_t*)p = lua_toboolean(L, -1) ? 1 : 0;
			break;
		case core_type_id::StdString:
			if (lua_isstring(L, -1))
				*(std::string*)p = lua_tostring(L, -1);
			break;
		default:
			break;
		}
	}
	lua_pop(L, 1);
}

// Walks every field in the class's lua_props_storage, runs destructors, returns the buffer to nullptr.
static void destroy_shadow_for(LuaClassTypeInfo* cti, Component* comp) {
	uint8_t* shadow = comp->get_lua_field_shadow();
	if (!shadow)
		return;
	for (auto& pi : cti->get_lua_props_storage())
		destruct_lua_field(pi, shadow + pi.offset);
	comp->take_lua_field_shadow(nullptr);
}

// Allocates + initializes a shadow buffer for `cti`, pulling defaults from the
// already-loaded template_lua_table. Caller must hand the result to the Component.
static std::unique_ptr<uint8_t[]> allocate_and_init_shadow(lua_State* L, LuaClassTypeInfo* cti) {
	if (cti->get_lua_field_shadow_size() == 0)
		return nullptr;
	auto buf = std::make_unique<uint8_t[]>(cti->get_lua_field_shadow_size());
	std::memset(buf.get(), 0, cti->get_lua_field_shadow_size());
	lua_rawgeti(L, LUA_REGISTRYINDEX, cti->get_template_lua_table());
	int tmpl_idx = lua_gettop(L);
	for (auto& pi : cti->get_lua_props_storage()) {
		uint8_t* p = buf.get() + pi.offset;
		construct_lua_field(pi, p);
		apply_template_default(L, tmpl_idx, pi, p);
	}
	lua_pop(L, 1);
	return buf;
}

LuaClassTypeInfo::LuaClassTypeInfo()
	: ClassTypeInfo("lua_class_empty", nullptr, nullptr, nullptr, false, nullptr, 0, nullptr, true) {
	this->is_lua_implemented = true;
}

LuaClassTypeInfo::~LuaClassTypeInfo() {}

void LuaClassTypeInfo::set_classname(string s) {
	this->lua_classname = s;
	this->classname = this->lua_classname.c_str();
}

bool LuaClassTypeInfo::set_superclass(string s) {
	auto find = ClassBase::find_class(s.c_str());
	if (!find) {
		sys_print(Error, "LuaClassTypeInfo: no super type %s\n", s.c_str());
		return false;
	} else if (!find->scriptable_allocate) {
		sys_print(Error, "LuaClassTypeInfo: super type isnt scriptable %s\n", s.c_str());
		return false;
	} else {
		this->super_typeinfo = find;
		this->superclassname = find->classname;
		this->lua_prototype_index_table = find->get_prototype_index_table();
		this->allocate = lua_class_alloc;
		return true;
	}
}

const string& LuaClassTypeInfo::get_name() {
	return lua_classname;
}

void LuaClassTypeInfo::synthesize_lua_props_unchecked_for_test() {
	// Mirror what ScriptManager::check_for_reload does: commit any pending parsed
	// properties staged by set_parsed_properties() before synthesis reads them.
	if (!pending_parsed_properties.empty() || !parsed_properties.empty())
		parsed_properties = std::move(pending_parsed_properties);
	synthesize_layout_from_parsed(this, lua_props_storage, lua_props_list,
								  lua_field_shadow_size, lua_classname, parsed_properties);
}

void LuaClassTypeInfo::synthesize_lua_props_for_component_subclass() {
	lua_props_storage.clear();
	lua_props_list = {};
	lua_field_shadow_size = 0;

	// Only synthesize for classes whose super chain reaches Component. We walk the
	// chain by pointer rather than calling ClassTypeInfo::is_a() because the latter
	// compares id ranges set by ClassBase::post_changes_class_init() — which on the
	// reload path runs AFTER init_lua_type(), so ids are still zeroed here.
	auto component_ti = ClassBase::find_class("Component");
	if (!component_ti)
		return;
	const ClassTypeInfo* p = this->super_typeinfo;
	while (p && p != component_ti)
		p = p->super_typeinfo;
	if (!p)
		return;
	synthesize_layout_from_parsed(this, lua_props_storage, lua_props_list,
								  lua_field_shadow_size, lua_classname, parsed_properties);
}

void LuaClassTypeInfo::init_lua_type() {
	auto L = ScriptManager::inst->get_lua_state();
	assert(lua_gettop(L) == 0);
	lua_getglobal(L, lua_classname.c_str());
	assert(lua_gettop(L) == 1);
	if (lua_isnil(L, -1)) {
		// class not found
		sys_print(Warning, "LuaClassTypeInfo::init_lua_type: class not found %s\n", lua_classname.c_str());
	} else {
		assert(lua_gettop(L) == 1);

		if (template_lua_table != 0)
			luaL_unref(L, LUA_REGISTRYINDEX, template_lua_table);
		assert(lua_gettop(L) == 1);
		template_lua_table = luaL_ref(L, LUA_REGISTRYINDEX);

		assert(lua_gettop(L) == 0);
		lua_pushnil(L);
		lua_setglobal(L, lua_classname.c_str());
		assert(lua_gettop(L) == 0);
		free_table_registry_id(); // free it if it exists
		assert(lua_gettop(L) == 0);
		ScriptManager::inst->init_this_class_type(this);
		assert(lua_gettop(L) == 0);
		ScriptManager::inst->set_class_type_global(this);
	}
	// Apply pending interface registrations (Lua-only, offset = -1)
	num_interfaces = 0; // reset before re-applying
	for (auto& iface_name : pending_interfaces) {
		auto* iface = InterfaceTypeInfo::find_interface(iface_name.c_str());
		if (iface) {
			add_interface(iface->id, -1);
		} else {
			sys_print(Warning, "LuaClassTypeInfo[%s]: unknown interface '%s'\n",
					  lua_classname.c_str(), iface_name.c_str());
		}
	}

	// Synthesize PROP_LUA_BACKED reflection from the most recent parse. Must happen AFTER
	// the metatable is rebuilt, BEFORE live-instance shadow buffers are reallocated by
	// the reload-merge path in check_for_reload().
	synthesize_lua_props_for_component_subclass();
	this->props = get_lua_props_list();
}

ClassBase* LuaClassTypeInfo::lua_class_alloc(const ClassTypeInfo* c) {
	assert(c->super_typeinfo && c->super_typeinfo->scriptable_allocate);
	auto out = c->super_typeinfo->scriptable_allocate(c);
	LuaClassTypeInfo* luaInfo = (LuaClassTypeInfo*)(c);
	auto L = ScriptManager::inst->get_lua_state();

	const int startTop = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, out->get_table_registry_id()); // -3
	lua_rawgeti(L, LUA_REGISTRYINDEX, luaInfo->template_lua_table);	 // -2

	auto check_top = [&](int v) {
		int topNow = lua_gettop(L);
		assert(topNow == startTop + v);
	};
	check_top(2);

	// Copy table1 into dst. In the editor, skip PROP_LUA_BACKED fields: those live in the
	// shadow buffer and must stay absent from the instance table so that
	// lua_component_index/__newindex (see ScriptManager.cpp) actually fire and serve the
	// live shadow value instead of a stale copy of the template's literal default -- the
	// editor property panel edits the shadow buffer directly, and this instance table copy
	// would otherwise permanently shadow (pun intended) those edits from Lua.
	// Outside the editor, PROP_LUA_BACKED fields are copied like everything else: a
	// runtime-spawned component (not loaded from a level file) never gets its shadow
	// buffer allocated (see comment below), so this copy is its only source of default
	// values -- sync_shadow_to_lua_table() no-ops when the shadow is null. Components that
	// *do* get a shadow (via level deserialization or the editor) simply have this default
	// overwritten later by sync_shadow_to_lua_table() before start() runs.
	const bool skip_lua_backed = eng && eng->is_editor_level();
	lua_pushnil(L); // first key
	check_top(3);

	while (lua_next(L, -2)) {
		check_top(4);
		bool is_lua_backed = false;
		if (skip_lua_backed && lua_isstring(L, -2)) {
			const char* key = lua_tostring(L, -2);
			for (auto& pi : luaInfo->get_lua_props_storage()) {
				if (strcmp(pi.name, key) == 0) {
					is_lua_backed = true;
					break;
				}
			}
		}
		if (!is_lua_backed) {
			lua_pushvalue(L, -2); // duplicate key
			lua_insert(L, -2);	  // move key below value
			lua_settable(L, -5); // dst[key] = value
								 // leaves key for next lua_next
		} else {
			lua_pop(L, 1); // pop value, leave key on stack for next lua_next
		}
	}
	check_top(2);
	check_top(2);

	lua_pop(L, 2);
	check_top(0);

	// Register every live Component instance (not just ones with PROP_LUA_BACKED fields) so
	// check_for_reload's live-instance walk can refresh already-spawned objects' methods on
	// reload -- see the function-recopy phase there. Shadow buffer itself is left null and
	// lazily allocated on first PROP_LUA_BACKED get_ptr() access (e.g. serialization, editor
	// property panel) so runtime-spawned components pay no allocation cost regardless.
	if (Component* comp = out->cast_to<Component>()) {
		comp->set_lua_owner_type(luaInfo);
		luaInfo->register_lua_instance(comp);
		if (luaInfo->is_init_in_editor_placeable())
			comp->set_call_init_in_editor(true);
	}
	return out;
}

void ScriptManager::check_for_reload() {
	if (!had_changes)
		return;
	lua_settop(lua, 0);

	// Find classes whose source changed this cycle (clears their per-class flag).
	std::vector<LuaClassTypeInfo*> changed;
	for (auto& [name, c] : lua_classes) {
		if (c->get_and_clear_had_changes())
			changed.push_back(c.get());
	}

	// Phase 1: snapshot every live instance's current field values against the OLD layout.
	std::unordered_map<LuaClassTypeInfo*, std::vector<LuaInstanceSnapshot>> per_class;
	for (auto* cti : changed) {
		auto& snaps = per_class[cti];
		for (auto* inst : cti->get_live_instances()) {
			LuaInstanceSnapshot snap;
			snap.inst = inst;
			uint8_t* shadow = inst->get_lua_field_shadow();
			if (shadow) {
				for (auto& pi : cti->get_lua_props_storage())
					snap.values.emplace(pi.name, snapshot_field(pi, shadow + pi.offset));
			}
			snaps.push_back(std::move(snap));
		}
	}

	// Phase 2: tear down each live instance's old shadow (runs string dtors) before
	// the class's lua_props_storage is rewritten by init_lua_type().
	for (auto& [cti, snaps] : per_class) {
		for (auto& snap : snaps)
			destroy_shadow_for(cti, snap.inst);
	}

	// Phase 2.5: commit any new parsed_properties staged by reload_from_content.
	// Must happen AFTER snapshot phase 1 (which read pi.name pointers into the old
	// parsed_properties storage) and BEFORE synthesize re-reads from parsed_properties.
	for (auto* cti : changed) {
		if (!cti->pending_parsed_properties.empty() || !cti->parsed_properties.empty())
			cti->parsed_properties = std::move(cti->pending_parsed_properties);
	}

	// Phase 3: rebuild type metadata (metatables + synthesized lua_props_storage).
	for (auto* cti : changed)
		cti->init_lua_type();

	// Phase 3.5: re-copy the fresh template's function entries onto every live instance's own
	// table, so already-spawned objects call the new implementations instead of the ones they
	// were constructed with. Deliberately skips non-function entries: field values are field
	// data, migrated separately (and more carefully, preserving live values) by Phase 4 below;
	// blindly overwriting them here would reintroduce template defaults over live/edited state.
	for (auto* cti : changed) {
		int tmpl_ref = cti->get_template_lua_table();
		if (tmpl_ref == 0 || cti->get_live_instances().empty())
			continue;
		lua_rawgeti(lua, LUA_REGISTRYINDEX, tmpl_ref);
		int tmpl_idx = lua_gettop(lua);
		for (auto* inst : cti->get_live_instances()) {
			lua_rawgeti(lua, LUA_REGISTRYINDEX, inst->get_table_registry_id());
			int inst_idx = lua_gettop(lua);
			lua_pushnil(lua);
			while (lua_next(lua, tmpl_idx)) {
				// stack: ... key, value
				if (lua_isfunction(lua, -1)) {
					lua_pushvalue(lua, -2); // dup key
					lua_pushvalue(lua, -2); // dup value (function)
					lua_rawset(lua, inst_idx);
				}
				lua_pop(lua, 1); // pop value, keep key for lua_next
			}
			lua_pop(lua, 1); // pop instance table
		}
		lua_pop(lua, 1); // pop template table
	}

	// Phase 4: reallocate each live instance's shadow against the NEW layout,
	// seed with template defaults, then overlay snapshot values where (name,type) match.
	for (auto& [cti, snaps] : per_class) {
		if (cti->get_lua_field_shadow_size() == 0) {
			// New class has no Lua-backed fields; live instances simply keep null shadow.
			continue;
		}
		lua_rawgeti(lua, LUA_REGISTRYINDEX, cti->get_template_lua_table());
		int tmpl_idx = lua_gettop(lua);
		for (auto& snap : snaps) {
			auto buf = std::make_unique<uint8_t[]>(cti->get_lua_field_shadow_size());
			std::memset(buf.get(), 0, cti->get_lua_field_shadow_size());
			for (auto& pi : cti->get_lua_props_storage()) {
				uint8_t* p = buf.get() + pi.offset;
				construct_lua_field(pi, p);
				apply_template_default(lua, tmpl_idx, pi, p);
				auto it = snap.values.find(pi.name);
				if (it != snap.values.end())
					restore_field(it->second, pi, p);
			}
			snap.inst->take_lua_field_shadow(std::move(buf));
		}
		lua_pop(lua, 1);
	}

	ClassBase::post_changes_class_init();
	had_changes = false;

	if (!changed.empty())
		on_class_reloaded.invoke();
}

static bool is_lua_shadow_field_type_supported(core_type_id type) {
	switch (type) {
	case core_type_id::Float:
	case core_type_id::Int32:
	case core_type_id::Enum32:
	case core_type_id::Bool:
	case core_type_id::StdString: return true;
	default: return false;
	}
}

void push_lua_shadow_field(lua_State* L, const PropertyInfo& pi, const uint8_t* p) {
	switch (pi.type) {
	case core_type_id::Float:   lua_pushnumber(L, *(const float*)p); break;
	case core_type_id::Int32:
	case core_type_id::Enum32:  lua_pushinteger(L, *(const int32_t*)p); break;
	case core_type_id::Bool:    lua_pushboolean(L, *(const int8_t*)p); break;
	case core_type_id::StdString: lua_pushstring(L, ((const std::string*)p)->c_str()); break;
	default: lua_pushnil(L); break;
	}
}

void write_lua_shadow_field(lua_State* L, int validx, const PropertyInfo& pi, uint8_t* p) {
	switch (pi.type) {
	case core_type_id::Float:   *(float*)p = (float)lua_tonumber(L, validx); break;
	case core_type_id::Int32:
	case core_type_id::Enum32:  *(int32_t*)p = (int32_t)lua_tointeger(L, validx); break;
	case core_type_id::Bool:    *(int8_t*)p = (int8_t)lua_toboolean(L, validx); break;
	case core_type_id::StdString: {
		const char* s = lua_tostring(L, validx);
		*(std::string*)p = s ? s : "";
		break;
	}
	default: break;
	}
}

// Pushes one shadow field value into the Lua instance table at tbl_idx.
static void push_field_to_lua_table(lua_State* L, int tbl_idx, const PropertyInfo& pi, const uint8_t* p) {
	if (!is_lua_shadow_field_type_supported(pi.type))
		return;
	lua_pushstring(L, pi.name);
	push_lua_shadow_field(L, pi, p);
	lua_rawset(L, tbl_idx);
}

void ScriptManager::ensure_shadow_for(Component* comp) {
	if (!inst)
		return;
	if (comp->get_lua_field_shadow())
		return;
	auto* lti = const_cast<LuaClassTypeInfo*>(comp->get_lua_owner_type());
	if (!lti || lti->get_lua_field_shadow_size() == 0)
		return;
	comp->take_lua_field_shadow(allocate_and_init_shadow(inst->lua, lti));
}

void ScriptManager::sync_shadow_to_lua_table(Component* comp) {
	if (!inst)
		return;
	const auto* lti = comp->get_lua_owner_type();
	if (!lti || lti->get_lua_field_shadow_size() == 0)
		return;
	const uint8_t* shadow = comp->get_lua_field_shadow();
	if (!shadow)
		return;
	lua_State* L = inst->lua;
	lua_rawgeti(L, LUA_REGISTRYINDEX, comp->get_table_registry_id());
	int tbl_idx = lua_gettop(L);
	for (const auto& pi : lti->get_lua_props_storage())
		push_field_to_lua_table(L, tbl_idx, pi, shadow + pi.offset);
	lua_pop(L, 1);
}

void ScriptManager::on_component_destructed(Component* c) {
	if (!ScriptManager::inst || !c)
		return;
	// Don't rely on c->get_type() here: by the time ~Component runs, the most-derived
	// (scriptable) destructor has already executed and the vtable has been downgraded
	// to Component's, so get_type() returns Component::StaticType. Use the cached
	// lua_owner_type pointer captured by lua_class_alloc instead.
	auto* lti = const_cast<LuaClassTypeInfo*>(c->get_lua_owner_type());
	if (!lti)
		return;
	destroy_shadow_for(lti, c);
	lti->unregister_lua_instance(c);
}
