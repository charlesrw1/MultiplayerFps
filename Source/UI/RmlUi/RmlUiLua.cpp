#include "RmlUiLua.h"
#include "RmlUiSystem.h"
#include "RmlUiDataModel.h"
#include "Framework/Log.h"
#include "Scripting/ScriptFunctionCodegen.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <unordered_map>
#include <vector>
#include <memory>
extern "C" {
#include <lauxlib.h>
}

namespace {

std::unordered_map<int, RmlGenericModel> g_models;
int g_next_model_id = 1;

RmlGenericModel* find_model(int id) {
	auto it = g_models.find(id);
	return it != g_models.end() ? &it->second : nullptr;
}

void variant_from_lua(lua_State* L, int idx, Rml::Variant& out) {
	switch (lua_type(L, idx)) {
	case LUA_TBOOLEAN: out = (bool)lua_toboolean(L, idx); break;
	case LUA_TNUMBER: out = (double)lua_tonumber(L, idx); break;
	case LUA_TSTRING: out = Rml::String(lua_tostring(L, idx)); break;
	default: out = Rml::String(""); break;
	}
}

void push_variant_to_lua(lua_State* L, const Rml::Variant& v) {
	switch (v.GetType()) {
	case Rml::Variant::NONE: lua_pushnil(L); break;
	case Rml::Variant::BOOL: lua_pushboolean(L, v.Get<bool>(false)); break;
	case Rml::Variant::STRING: lua_pushstring(L, v.Get<Rml::String>().c_str()); break;
	default: lua_pushnumber(L, v.Get<double>(0.0)); break;
	}
}

// Reads a flat {key=value, ...} Lua table (string keys, scalar values) into
// a data-model row - the "cards" use case's {image=.., text=..} shape.
void lua_table_to_row(lua_State* L, int idx, RmlGenericRow& row) {
	luaL_checktype(L, idx, LUA_TTABLE);
	idx = lua_absindex(L, idx);
	lua_pushnil(L);
	while (lua_next(L, idx) != 0) {
		// key at -2, value at -1
		if (lua_type(L, -2) == LUA_TSTRING) {
			Rml::Variant v;
			variant_from_lua(L, -1, v);
			row.fields[lua_tostring(L, -2)] = std::move(v);
		}
		lua_pop(L, 1); // pop value, keep key for next iteration
	}
}

// Ensures a scalar/array field is bound into the RmlUi data model the first
// time Lua touches it - lazy binding, since Lua tables (and therefore model
// shapes) aren't known upfront. Re-fetching GetDataModel() is cheap and
// valid at any point per RmlUi's own docs ("can be used to add additional
// bindings to an existing model").
Rml::DataModelConstructor get_model_constructor(RmlGenericModel& model) {
	return RmlUiSystem::inst->get_context()->GetDataModel(model.name);
}

void ensure_scalar_bound(RmlGenericModel& model, const std::string& field) {
	if (model.scalars.find(field) != model.scalars.end())
		return;
	Rml::Variant& slot = model.scalars[field];
	Rml::DataModelConstructor ctor = get_model_constructor(model);
	ctor.BindCustomDataVariable(field, Rml::DataVariable(rmlui_generic_scalar_definition(), &slot));
}

void ensure_array_bound(RmlGenericModel& model, const std::string& field) {
	if (model.arrays.find(field) != model.arrays.end())
		return;
	std::vector<RmlGenericRow>& slot = model.arrays[field];
	Rml::DataModelConstructor ctor = get_model_constructor(model);
	ctor.BindCustomDataVariable(field, Rml::DataVariable(rmlui_generic_array_definition(), &slot));
}

// Forwards a bound RmlUi element event into a stored Lua function reference,
// using the same luaL_ref(LUA_REGISTRYINDEX) idiom ScriptManager uses for
// object handles (see ScriptManager::create_class_table_for).
class LuaEventListener : public Rml::EventListener {
public:
	LuaEventListener(lua_State* L, int ref) : L(L), ref(ref) {}
	~LuaEventListener() override { luaL_unref(L, LUA_REGISTRYINDEX, ref); }

	void ProcessEvent(Rml::Event& event) override {
		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
		if (!lua_isfunction(L, -1)) {
			lua_pop(L, 1);
			return;
		}
		lua_pushstring(L, event.GetType().c_str());
		lua_pushnumber(L, event.GetParameter<float>("mouse_x", 0.f));
		lua_pushnumber(L, event.GetParameter<float>("mouse_y", 0.f));
		if (safe_pcall(L, 3, 0) != LUA_OK) {
			sys_print(Error, "RmlUi event callback error: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}

private:
	lua_State* L;
	int ref;
};

// Kept alive for the lifetime of the Lua state; RmlUi documents/elements
// this is bound to are always closed before the state tears down.
std::vector<std::unique_ptr<LuaEventListener>> g_listeners;

Rml::Element* find_element(Rml::ElementDocument* doc, const char* selector_or_id) {
	if (Rml::Element* e = doc->GetElementById(selector_or_id))
		return e;
	return doc->QuerySelector(selector_or_id);
}

// ---- lua_CFunction bindings ------------------------------------------

int l_load_document(lua_State* L) {
	const char* path = luaL_checkstring(L, 1);
	lua_pushinteger(L, RmlUiSystem::inst->load_document(path));
	return 1;
}
int l_show_document(lua_State* L) {
	RmlUiSystem::inst->show_document((int)luaL_checkinteger(L, 1));
	return 0;
}
int l_hide_document(lua_State* L) {
	RmlUiSystem::inst->hide_document((int)luaL_checkinteger(L, 1));
	return 0;
}
int l_close_document(lua_State* L) {
	RmlUiSystem::inst->close_document((int)luaL_checkinteger(L, 1));
	return 0;
}

int l_create_data_model(lua_State* L) {
	const char* name = luaL_checkstring(L, 1);
	Rml::DataModelConstructor ctor = RmlUiSystem::inst->get_context()->CreateDataModel(name);
	if (!ctor)
		return luaL_error(L, "RmlUi.create_data_model: failed to create model '%s' (name already in use?)", name);
	const int id = g_next_model_id++;
	RmlGenericModel& model = g_models[id];
	model.name = name;
	model.handle = ctor.GetModelHandle();
	lua_pushinteger(L, id);
	return 1;
}

int l_set_value(lua_State* L) {
	RmlGenericModel* model = find_model((int)luaL_checkinteger(L, 1));
	if (!model)
		return luaL_error(L, "RmlUi.set_value: invalid model id");
	const std::string field = luaL_checkstring(L, 2);
	ensure_scalar_bound(*model, field);
	variant_from_lua(L, 3, model->scalars[field]);
	model->handle.DirtyVariable(field);
	return 0;
}

int l_get_value(lua_State* L) {
	RmlGenericModel* model = find_model((int)luaL_checkinteger(L, 1));
	if (!model)
		return luaL_error(L, "RmlUi.get_value: invalid model id");
	const std::string field = luaL_checkstring(L, 2);
	auto it = model->scalars.find(field);
	if (it == model->scalars.end()) {
		lua_pushnil(L);
		return 1;
	}
	push_variant_to_lua(L, it->second);
	return 1;
}

int l_array_push(lua_State* L) {
	RmlGenericModel* model = find_model((int)luaL_checkinteger(L, 1));
	if (!model)
		return luaL_error(L, "RmlUi.array_push: invalid model id");
	const std::string field = luaL_checkstring(L, 2);
	ensure_array_bound(*model, field);
	RmlGenericRow row;
	lua_table_to_row(L, 3, row);
	model->arrays[field].push_back(std::move(row));
	model->handle.DirtyVariable(field);
	return 0;
}

int l_array_erase(lua_State* L) {
	RmlGenericModel* model = find_model((int)luaL_checkinteger(L, 1));
	if (!model)
		return luaL_error(L, "RmlUi.array_erase: invalid model id");
	const std::string field = luaL_checkstring(L, 2);
	const int index = (int)luaL_checkinteger(L, 3);
	auto it = model->arrays.find(field);
	if (it != model->arrays.end() && index >= 0 && index < (int)it->second.size()) {
		it->second.erase(it->second.begin() + index);
		model->handle.DirtyVariable(field);
	}
	return 0;
}

int l_array_set(lua_State* L) {
	RmlGenericModel* model = find_model((int)luaL_checkinteger(L, 1));
	if (!model)
		return luaL_error(L, "RmlUi.array_set: invalid model id");
	const std::string field = luaL_checkstring(L, 2);
	const int index = (int)luaL_checkinteger(L, 3);
	ensure_array_bound(*model, field);
	auto& arr = model->arrays[field];
	if (index < 0 || index >= (int)arr.size())
		return luaL_error(L, "RmlUi.array_set: index %d out of bounds (size %d)", index, (int)arr.size());
	arr[index] = RmlGenericRow{};
	lua_table_to_row(L, 4, arr[index]);
	model->handle.DirtyVariable(field);
	return 0;
}

int l_set_element_property(lua_State* L) {
	Rml::ElementDocument* doc = RmlUiSystem::inst->get_document((int)luaL_checkinteger(L, 1));
	if (!doc)
		return luaL_error(L, "RmlUi.set_element_property: invalid document handle");
	Rml::Element* elem = find_element(doc, luaL_checkstring(L, 2));
	if (!elem)
		return luaL_error(L, "RmlUi.set_element_property: element not found: %s", lua_tostring(L, 2));
	const char* prop_name = luaL_checkstring(L, 3);
	const char* prop_value = luaL_checkstring(L, 4);
	if (!elem->SetProperty(prop_name, prop_value))
		sys_print(Warning, "RmlUi.set_element_property: invalid property '%s: %s'\n", prop_name, prop_value);
	return 0;
}

int l_bind_event(lua_State* L) {
	Rml::ElementDocument* doc = RmlUiSystem::inst->get_document((int)luaL_checkinteger(L, 1));
	if (!doc)
		return luaL_error(L, "RmlUi.bind_event: invalid document handle");
	Rml::Element* elem = find_element(doc, luaL_checkstring(L, 2));
	if (!elem)
		return luaL_error(L, "RmlUi.bind_event: element not found: %s", lua_tostring(L, 2));
	const char* event_name = luaL_checkstring(L, 3);
	luaL_checktype(L, 4, LUA_TFUNCTION);
	lua_pushvalue(L, 4);
	const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
	auto listener = std::make_unique<LuaEventListener>(L, ref);
	elem->AddEventListener(event_name, listener.get());
	g_listeners.push_back(std::move(listener));
	return 0;
}

const luaL_Reg RMLUI_FUNCS[] = {
	{"load_document", l_load_document},
	{"show_document", l_show_document},
	{"hide_document", l_hide_document},
	{"close_document", l_close_document},
	{"create_data_model", l_create_data_model},
	{"set_value", l_set_value},
	{"get_value", l_get_value},
	{"array_push", l_array_push},
	{"array_erase", l_array_erase},
	{"array_set", l_array_set},
	{"set_element_property", l_set_element_property},
	{"bind_event", l_bind_event},
	{nullptr, nullptr},
};

} // namespace

void register_rmlui_lua(lua_State* L) {
	luaL_newlib(L, RMLUI_FUNCS);
	lua_setglobal(L, "RmlUi");
}

void rmlui_lua_reset() {
	g_models.clear();
	g_listeners.clear();
}
