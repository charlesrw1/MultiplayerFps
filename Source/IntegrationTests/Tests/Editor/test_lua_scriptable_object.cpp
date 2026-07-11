// Source/IntegrationTests/Tests/Editor/test_lua_scriptable_object.cpp
//
// Editor-mode integration tests for Lua-defined ScriptableObject subclasses (.sobj assets):
//   1. Newly-created .sobj file is discovered by the asset browser/registry.
//   2. A Lua ScriptableObject subclass's ---@type fields are synthesized into a
//      PropertyInfoList (regression: synthesis used to be gated to Component-derived
//      classes only, leaving ScriptableObject subclasses with an empty prop list).
//   3. Edit a field, save to disk, then hot-reload the in-memory asset the same way
//      AssetRegistrySystem::update() does on a file-watcher event (regression: reload
//      reused the same instance in place and cleared lua_owner_type in uninstall(),
//      so ensure_shadow_for silently no-op'd and PropertyInfo::get_ptr asserted on a
//      null shadow buffer during deserialization).
//   4. Lua *script code* reading `self.field` on a ScriptableObject instance (as opposed
//      to C++ PropertyInfo accessors) must see the live shadow-buffer value (regression:
//      the __index/__newindex metamethod's shared helper hard-cast `self` to Component*,
//      so it silently fell through to nil for ScriptableObject-derived instances).
//   5. self.field writes persist across repeated calls rather than resetting.
//   6. ScriptableObject::load(type, path), the lua_generic static factory/loader, works both
//      as a direct C++ call and as `ScriptableObject.load(SomeType, path)` from Lua.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Framework/ClassBase.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Files.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetRegistryLocal.h"
#include "Assets/AssetDatabase.h"
#include "Assets/ScriptableObject.h"
#include "AssetTools/AssetTemplates.h"
#include "Scripting/ScriptManager.h"
#include <algorithm>
#include <string>

namespace {

constexpr const char* kLuaSrc =
	"---@class TestLuaSobj : ScriptableObject\n"
	"TestLuaSobj = {\n"
	"  ---@type integer\n"
	"  num = 0,\n"
	"  ---@type number\n"
	"  floating = 0.0,\n"
	"}\n";

void load_lua_class() {
	ScriptManager::inst->reload_from_content(kLuaSrc, "test_lua_scriptable_object");
	ScriptManager::inst->check_for_reload();
}

const PropertyInfo* find_field_sobj(const ClassTypeInfo* ti, const char* name) {
	if (!ti || !ti->props)
		return nullptr;
	for (int i = 0; i < ti->props->count; ++i) {
		auto& pi = ti->props->list[i];
		if ((pi.flags & PROP_LUA_BACKED) && std::string(pi.name) == name)
			return &pi;
	}
	return nullptr;
}

} // namespace

// 1. Creating a .sobj file must make it appear in the asset registry's linear list,
//    typed as ScriptableObject — the browser's directory scan/watcher previously had
//    a hardcoded extension chain that never included "sobj".
static TestTask test_sobj_appears_in_registry(TestContext& t) {
	const std::string kName = "_integration_test_dummy.sobj";
	FileSys::delete_game_file(kName);
	t.check(!FileSys::does_file_exist(kName.c_str(), FileSys::GAME_DIR), "file not on disk yet");
	co_await t.wait_ticks(3);

	auto created = AssetTemplates::create_scriptable_object("", "_integration_test_dummy", "ScriptableObject");
	t.require(created.has_value(), "create_scriptable_object succeeded");
	co_await t.wait_seconds(0.5f);

	auto try_find = [&]() {
		auto& all_files = AssetRegistrySystem::get().get_linear_list();
		return std::any_of(all_files.begin(), all_files.end(), [&](AssetFilesystemNode* n) {
			assert(n);
			return n->asset.filename == kName && n->asset.type->get_asset_class_type() == &ScriptableObject::StaticType;
		});
	};
	const bool found_in = try_find();
	FileSys::delete_game_file(kName);
	t.check(found_in, "newly created .sobj found in asset registry as ScriptableObject");

	co_await t.wait_seconds(0.5f);
	t.check(!try_find(), "deleted .sobj removed from asset registry");
}
EDITOR_TEST("editor/sobj_appears_in_registry", 15.f, test_sobj_appears_in_registry);

// 2. A Lua-defined ScriptableObject subclass must synthesize a non-empty PropertyInfoList
//    (previously gated to Component-derived classes only).
static TestTask test_sobj_lua_props_synthesized(TestContext& t) {
	load_lua_class();
	co_await t.wait_ticks(1);

	auto* ti = ClassBase::find_class("TestLuaSobj");
	t.require(ti != nullptr, "TestLuaSobj registered after reload_from_content");
	t.require(ti->props != nullptr, "TestLuaSobj synthesized a PropertyInfoList");
	t.check(ti->props->count == 2, "TestLuaSobj exposes 2 fields (num, floating)");
	t.check(find_field_sobj(ti, "num") != nullptr, "num field reflected");
	t.check(find_field_sobj(ti, "floating") != nullptr, "floating field reflected");
}
EDITOR_TEST("editor/sobj_lua_props_synthesized", 15.f, test_sobj_lua_props_synthesized);

// 3. Edit -> save -> hot-reload (in-place, same instance) must not crash and must
//    round-trip the edited values. This exercises the exact path AssetRegistrySystem::
//    update() takes on a file-watcher event: AssetDatabaseImpl::reload_asset_sync calls
//    uninstall() then load_asset() on the SAME ScriptableObject instance.
static TestTask test_sobj_edit_save_hotreload(TestContext& t) {
	load_lua_class();
	auto* ti = ClassBase::find_class("TestLuaSobj");
	t.require(ti != nullptr, "TestLuaSobj registered");

	const std::string kName = "_integration_test_sobj_reload.sobj";
	FileSys::delete_game_file(kName);
	auto created = AssetTemplates::create_scriptable_object("", "_integration_test_sobj_reload", "TestLuaSobj");
	t.require(created.has_value(), "create_scriptable_object succeeded");
	co_await t.wait_ticks(1);

	auto asset = g_assets.find<ScriptableObject>(kName);
	t.require(asset.get_unsafe() != nullptr, "loaded .sobj instance");
	t.require(&asset.get_unsafe()->get_type() == ti, "loaded instance is concrete TestLuaSobj type");

	auto* num_pi = find_field_sobj(ti, "num");
	auto* floating_pi = find_field_sobj(ti, "floating");
	t.require(num_pi && floating_pi, "both fields found on loaded instance");

	num_pi->set_int(asset.get_unsafe(), 42);
	floating_pi->set_float(asset.get_unsafe(), 3.5f);
	co_await t.wait_ticks(1);

	static_cast<ScriptableObject*>(asset.get_unsafe())->save_to_disk();
	co_await t.wait_ticks(1);

	// Reproduce the crash path directly: uninstall() + load_asset() on the same instance,
	// exactly what AssetDatabaseImpl::reload_asset_sync does on a file-watcher hot-reload.
	g_assets.reload<ScriptableObject>(asset);
	co_await t.wait_ticks(1);

	t.check(num_pi->get_int(asset.get_unsafe()) == 42, "num survived edit/save/hot-reload");
	t.check(floating_pi->get_float(asset.get_unsafe()) == 3.5f, "floating survived edit/save/hot-reload");

	FileSys::delete_game_file(kName);
}
EDITOR_TEST("editor/sobj_edit_save_hotreload", 20.f, test_sobj_edit_save_hotreload);

// 4. self.field reads inside Lua script code (e.g. an on_property_change() override, the
//    exact repro reported: "attempt to concatenate a nil value (field 'floating')") must
//    resolve to the shadow buffer, matching what the C++ PropertyInfo accessors already see.
constexpr const char* kLuaSrcSelfAccess =
	"---@class TestLuaSobjSelfAccess : ScriptableObject\n"
	"TestLuaSobjSelfAccess = {\n"
	"  ---@type number\n"
	"  floating = 0.0,\n"
	"  ---@type string\n"
	"  read_back = \"\",\n"
	"}\n"
	"function TestLuaSobjSelfAccess:on_property_change()\n"
	"  self.read_back = \"got:\" .. self.floating\n"
	"end\n";

static TestTask test_sobj_self_field_access_from_lua(TestContext& t) {
	ScriptManager::inst->reload_from_content(kLuaSrcSelfAccess, "test_lua_scriptable_object_self_access");
	ScriptManager::inst->check_for_reload();
	co_await t.wait_ticks(1);

	auto* ti = ClassBase::find_class("TestLuaSobjSelfAccess");
	t.require(ti != nullptr, "TestLuaSobjSelfAccess registered");

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "editor level loaded");

	auto* floating_pi = find_field_sobj(ti, "floating");
	auto* read_back_pi = find_field_sobj(ti, "read_back");
	t.require(floating_pi && read_back_pi, "both fields found");

	std::unique_ptr<ClassBase> instance(ti->allocate_this_type());
	t.require(instance != nullptr, "allocated TestLuaSobjSelfAccess instance");
	auto* obj = static_cast<ScriptableObject*>(instance.get());

	floating_pi->set_float(obj, 3.5f);

	// Dispatches to the Lua-defined override; reads self.floating from inside Lua script code.
	// Pre-fix this would throw "attempt to concatenate a nil value (field 'floating')".
	obj->on_property_change();
	co_await t.wait_ticks(1);

	auto* read_back = (std::string*)read_back_pi->get_ptr(obj);
	t.check(*read_back == "got:3.5", "self.floating was readable from Lua script code");
}
EDITOR_TEST("editor/sobj_self_field_access_from_lua", 20.f, test_sobj_self_field_access_from_lua);

// 5. Repeated self.num = self.num + 1 across multiple on_property_change() calls (the exact
//    user-reported repro) must persist and monotonically increment -- not reset to the
//    template default on every call. Loads a real .sobj from disk, same as the inspector,
//    rather than a bare in-memory instance.
constexpr const char* kLuaSrcIncrement =
	"---@class TestLuaSobjIncrement : ScriptableObject\n"
	"TestLuaSobjIncrement = {\n"
	"  ---@type integer\n"
	"  num = 0,\n"
	"  ---@type number\n"
	"  floating = 0.0,\n"
	"}\n"
	"function TestLuaSobjIncrement:on_property_change()\n"
	"  self.num = self.num + 1\n"
	"end\n";

static TestTask test_sobj_self_field_increment_persists(TestContext& t) {
	ScriptManager::inst->reload_from_content(kLuaSrcIncrement, "test_lua_scriptable_object_increment");
	ScriptManager::inst->check_for_reload();
	co_await t.wait_ticks(1);

	auto* ti = ClassBase::find_class("TestLuaSobjIncrement");
	t.require(ti != nullptr, "TestLuaSobjIncrement registered");

	const std::string kName = "_integration_test_sobj_increment.sobj";
	FileSys::delete_game_file(kName);
	auto created = AssetTemplates::create_scriptable_object("", "_integration_test_sobj_increment", "TestLuaSobjIncrement");
	t.require(created.has_value(), "create_scriptable_object succeeded");
	co_await t.wait_ticks(1);

	auto asset = g_assets.find<ScriptableObject>(kName);
	t.require(asset.get_unsafe() != nullptr, "loaded .sobj instance");

	auto* num_pi = find_field_sobj(ti, "num");
	t.require(num_pi != nullptr, "num field found on loaded instance");
	t.check(num_pi->get_int(asset.get_unsafe()) == 0, "num starts at template default 0");

	asset.get_unsafe()->on_property_change();
	co_await t.wait_ticks(1);
	t.check(num_pi->get_int(asset.get_unsafe()) == 1, "num incremented to 1 after first call");

	asset.get_unsafe()->on_property_change();
	co_await t.wait_ticks(1);
	t.check(num_pi->get_int(asset.get_unsafe()) == 2, "num incremented to 2 after second call (not reset)");

	asset.get_unsafe()->on_property_change();
	co_await t.wait_ticks(1);
	t.check(num_pi->get_int(asset.get_unsafe()) == 3, "num incremented to 3 after third call (not reset)");

	FileSys::delete_game_file(kName);
}
EDITOR_TEST("editor/sobj_self_field_increment_persists", 20.f, test_sobj_self_field_increment_persists);

// 6. ScriptableObject::load(type, path) -- the generic (lua_generic) static factory/loader.
//    Covers both call surfaces: direct C++ call, and calling it from Lua script code as
//    `ScriptableObject.load(SomeType, "path")` where the returned object's fields are then
//    read back via self.field (dogfoods the earlier __index fix on the loaded instance too).
constexpr const char* kLuaSrcLoader =
	"---@class TestLuaSobjLoader : ScriptableObject\n"
	"TestLuaSobjLoader = {\n"
	"  ---@type integer\n"
	"  loaded_num = -1,\n"
	"}\n"
	"function TestLuaSobjLoader:on_property_change()\n"
	"  local target = ScriptableObject.load(TestLuaSobj, \"_integration_test_sobj_load_target.sobj\")\n"
	"  self.loaded_num = target.num\n"
	"end\n";

static TestTask test_sobj_load_static_factory(TestContext& t) {
	load_lua_class(); // registers TestLuaSobj (num, floating), referenced by name from kLuaSrcLoader
	ScriptManager::inst->reload_from_content(kLuaSrcLoader, "test_lua_scriptable_object_loader");
	ScriptManager::inst->check_for_reload();
	co_await t.wait_ticks(1);

	auto* target_ti = ClassBase::find_class("TestLuaSobj");
	auto* loader_ti = ClassBase::find_class("TestLuaSobjLoader");
	t.require(target_ti != nullptr && loader_ti != nullptr, "both Lua classes registered");

	const std::string kTargetName = "_integration_test_sobj_load_target.sobj";
	FileSys::delete_game_file(kTargetName);
	auto created = AssetTemplates::create_scriptable_object("", "_integration_test_sobj_load_target", "TestLuaSobj");
	t.require(created.has_value(), "create_scriptable_object succeeded");
	co_await t.wait_ticks(1);

	auto target_asset = g_assets.find<ScriptableObject>(kTargetName);
	t.require(target_asset.get_unsafe() != nullptr, "target .sobj loaded");
	auto* num_pi = find_field_sobj(target_ti, "num");
	t.require(num_pi != nullptr, "num field found on target");
	num_pi->set_int(target_asset.get_unsafe(), 7);
	static_cast<ScriptableObject*>(target_asset.get_unsafe())->save_to_disk();
	co_await t.wait_ticks(1);

	// Direct C++ call surface.
	auto* loaded_via_cpp = ScriptableObject::load(target_ti, kTargetName);
	t.require(loaded_via_cpp != nullptr, "ScriptableObject::load found the target asset");
	t.check(num_pi->get_int(loaded_via_cpp) == 7, "loaded instance has the saved value");
	t.check(&loaded_via_cpp->get_type() == target_ti, "loaded instance is the concrete TestLuaSobj type");

	// Lua call surface: TestLuaSobjLoader.on_property_change() calls
	// ScriptableObject.load(TestLuaSobj, path) and reads target.num back via self.field.
	std::unique_ptr<ClassBase> loader_instance(loader_ti->allocate_this_type());
	t.require(loader_instance != nullptr, "allocated TestLuaSobjLoader instance");
	auto* loader_obj = static_cast<ScriptableObject*>(loader_instance.get());

	loader_obj->on_property_change();
	co_await t.wait_ticks(1);

	auto* loaded_num_pi = find_field_sobj(loader_ti, "loaded_num");
	t.require(loaded_num_pi != nullptr, "loaded_num field found");
	t.check(loaded_num_pi->get_int(loader_obj) == 7,
			"Lua-called ScriptableObject.load() returned an object whose .num field read back correctly");

	FileSys::delete_game_file(kTargetName);
}
EDITOR_TEST("editor/sobj_load_static_factory", 20.f, test_sobj_load_static_factory);
