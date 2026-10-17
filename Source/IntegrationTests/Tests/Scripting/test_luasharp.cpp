// Source/IntegrationTests/Tests/Scripting/test_luasharp.cpp
//
// LuaSharp components in the running engine. The classes are in
// TestFilesData/scripts/tests/luasharp_spinner.ls, which the engine loads at startup in test mode.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/EditorTestContext.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Framework/ClassBase.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Files.h"
#include "Scripting/LuaSharpEngine.h"
#include "../LuaSharp/src/CBackend.h"
#include "LevelEditor/EditorDocLocal.h"
#include <string>
#ifdef EDITOR_BUILD
#include "LevelEditor/EdPropertyGrid.h"
#include "LevelEditor/PropertyEditors.h"
#include "Framework/FnFactory.h"
#include "Framework/PropertyEd.h"
#endif

namespace {

constexpr const char* kModule = "scripts/tests/luasharp_spinner.ls";

const luasharp::NativeModule* spinner_module() {
	return LuaSharpEngine::inst ? LuaSharpEngine::inst->find_module(kModule) : nullptr;
}

int64_t module_int(const char* name) {
	const auto* module = spinner_module();
	ASSERT(module);
	const auto* value = static_cast<const int64_t*>(module->FindGlobal(name));
	ASSERT(value);
	return *value;
}

// Finds the property in the class or its bases.
const PropertyInfo* find_prop(const ClassTypeInfo* type, std::string_view name) {
	for (; type; type = type->super_typeinfo) {
		if (!type->props)
			continue;
		for (int i = 0; i < type->props->count; i++)
			if (type->props->list[i].name == name)
				return &type->props->list[i];
	}
	return nullptr;
}

template <typename T> T& field(ClassBase* object, std::string_view name) {
	const PropertyInfo* prop = find_prop(&object->get_type(), name);
	ASSERT(prop && (prop->flags & PROP_SCRIPT_FIELD));
	return *reinterpret_cast<T*>(prop->get_ptr(object));
}

std::string label_of(ClassBase* object) { return find_prop(&object->get_type(), "label")->get_script_string(object); }

Component* find_component_of_type(const ClassTypeInfo* type) {
	for (auto obj : eng->get_level()->get_all_objects())
		if (auto* c = obj ? obj->cast_to<Component>() : nullptr; c && &c->get_type() == type)
			return c;
	return nullptr;
}

} // namespace

// The engine loads the .ls file and registers its classes; their [Editable] fields are PropertyInfos.
static TestTask test_luasharp_classes_registered(TestContext& t) {
	t.require(LuaSharpEngine::inst != nullptr, "LuaSharpEngine is created at startup");
	t.require(spinner_module() != nullptr, "luasharp_spinner.ls is loaded");

	const ClassTypeInfo* spinner = ClassBase::find_class("LsTestSpinner");
	const ClassTypeInfo* fast = ClassBase::find_class("LsTestFastSpinner");
	t.require(spinner && fast, "both script classes are registered");
	t.check(spinner->is_a(Component::StaticType), "LsTestSpinner is a Component");
	t.check(fast->super_typeinfo == spinner, "LsTestFastSpinner derives from LsTestSpinner");
	t.check(spinner->props && spinner->props->count == 5, "LsTestSpinner has 5 [Editable] fields");
	t.check(fast->props && fast->props->count == 1, "LsTestFastSpinner declares 1 [Editable] field");
	t.check(find_prop(spinner, "speed")->type == core_type_id::Double, "float is Double");
	t.check(find_prop(spinner, "label")->type == core_type_id::ScriptString, "string is ScriptString");
	t.check(find_prop(spinner, "step")->type == core_type_id::Vec3, "Vec3 is Vec3");
	co_return;
}
GAME_TEST("luasharp/classes_registered", 10.f, test_luasharp_classes_registered);

// C++ spawns the script component; the engine calls the script overrides of start, update and stop.
static TestTask test_luasharp_component_lifecycle(TestContext& t) {
	t.require(spinner_module() != nullptr, "luasharp_spinner.ls is loaded");
	eng->load_level("");
	co_await t.wait_ticks(1);
	const int64_t starts0 = module_int("Counters.starts");
	const int64_t stops0 = module_int("Counters.stops");

	Entity* e = eng->get_level()->spawn_entity();
	e->set_ws_position(glm::vec3(0.f));
	Component* comp = e->create_component(ClassBase::find_class("LsTestSpinner"));
	t.require(comp != nullptr, "create_component made the script component");
	t.check(&comp->get_type() == ClassBase::find_class("LsTestSpinner"), "get_type() is the script class");
	t.check(LuaSharpEngine::inst->proxy_of(comp) != nullptr, "the component has its script object");

	// start(): the script changed a field, called set_ticking and wrote a field of the owner entity.
	t.check(module_int("Counters.starts") == starts0 + 1, "start() override ran");
	t.check(label_of(comp) == "started", "start() wrote the label field");
	t.check(e->get_editor_name() == "started", "start() wrote Entity.editor_name through the bound field");
	t.check(field<double>(comp, "speed") == 2.0, "the field initializer ran");

	// update(): ticked by the level after set_ticking(true) in the script.
	field<double>(comp, "speed") = 0.5; // the same write the property editor does
	co_await t.wait_ticks(5);
	const int64_t ticks = field<int64_t>(comp, "ticks");
	t.check(ticks >= 4, "update() override ran each frame");
	t.check(e->get_ws_position().x == static_cast<float>(ticks) * 0.5f, "update() moved the entity by step * speed");

	// A virtual call from C++ goes to the script override too.
	comp->update();
	t.check(field<int64_t>(comp, "ticks") == ticks + 1, "a C++ virtual call ran the script override");

	field<uint8_t>(comp, "spinning") = 0;
	const int64_t frozen = field<int64_t>(comp, "ticks");
	co_await t.wait_ticks(3);
	t.check(field<int64_t>(comp, "ticks") == frozen, "the script reads fields that C++ wrote");

	// The collector keeps the objects that components and modules reference.
	LuaSharpEngine::inst->collect();
	t.check(label_of(comp) == "started", "the label string survives a collection");

	// stop(): runs when the entity is destroyed.
	e->destroy();
	co_await t.wait_ticks(2);
	t.check(module_int("Counters.stops") == stops0 + 1, "stop() override ran on destroy");
	LuaSharpEngine::inst->collect();
}
GAME_TEST("luasharp/component_lifecycle", 20.f, test_luasharp_component_lifecycle);

// A script class that derives from a script class: its override calls the script base with base.update().
static TestTask test_luasharp_derived_script_class(TestContext& t) {
	t.require(spinner_module() != nullptr, "luasharp_spinner.ls is loaded");
	eng->load_level("");
	co_await t.wait_ticks(1);

	Entity* e = eng->get_level()->spawn_entity();
	Component* comp = e->create_component(ClassBase::find_class("LsTestFastSpinner"));
	t.require(comp != nullptr, "create_component made the derived script component");
	t.check(comp->get_type().is_a(*ClassBase::find_class("LsTestSpinner")), "is_a the script base class");
	t.check(label_of(comp) == "started", "the inherited start() override ran");

	field<double>(comp, "speed") = 1.0;
	field<int64_t>(comp, "boost") = 10;
	const int64_t before = field<int64_t>(comp, "ticks");
	comp->update();
	t.check(field<int64_t>(comp, "ticks") == before + 11, "the override called base.update() and added boost");
	e->destroy();
	co_await t.wait_ticks(1);
}
GAME_TEST("luasharp/derived_script_class", 20.f, test_luasharp_derived_script_class);

// Place the component in the editor, edit its fields, save, reopen: the fields are serialized.
static TestTask test_luasharp_editor_save_reload(TestContext& t) {
	t.require(spinner_module() != nullptr, "luasharp_spinner.ls is loaded");
	const ClassTypeInfo* type = ClassBase::find_class("LsTestFastSpinner");
	t.require(type != nullptr, "LsTestFastSpinner registered");

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "blank editor level loaded");
	auto* doc = static_cast<EditorDoc*>(eng->get_tool());
	t.require(doc != nullptr, "got EditorDoc");

	const int64_t editor_starts0 = module_int("Counters.editor_starts");
	Entity* e = doc->spawn_entity();
	Component* comp = doc->attach_component(type, e);
	t.require(comp != nullptr, "attached the script component in the editor");
	t.check(module_int("Counters.editor_starts") == editor_starts0 + 1, "editor_start() override ran");

	field<double>(comp, "speed") = 0.25;
	field<int64_t>(comp, "boost") = 42;
	field<uint8_t>(comp, "spinning") = 0;
	field<glm::vec3>(comp, "step") = glm::vec3(4, 5, 6);
	find_prop(type, "label")->set_script_string(comp, "saved label");

#ifdef EDITOR_BUILD
	// The property grid makes a row for each [Editable] field.
	FnFactory<IPropertyEditor> factory;
	PropertyFactoryUtil::register_basic(factory);
	PropertyGrid grid(factory);
	grid.add_class_to_grid(comp);
	int script_rows = 0;
	for (auto& row : grid.rows)
		if (auto* group = dynamic_cast<GroupRow*>(row.get()))
			if (group->proplist == type->props || group->proplist == type->super_typeinfo->props)
				script_rows += static_cast<int>(group->child_rows.size());
	t.check(script_rows == 6, "the property grid shows the 6 [Editable] fields");
#endif

	const char* kPath = "_temp_luasharp_component.tmap";
	FileSys::delete_game_file(kPath);
	t.editor().save_level(kPath);
	co_await t.wait_ticks(2);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + kPath).c_str());
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "reopened level");

	Component* loaded = find_component_of_type(type);
	t.require(loaded != nullptr, "the script component was loaded from the map");
	t.check(field<double>(loaded, "speed") == 0.25, "speed survived save and load");
	t.check(field<int64_t>(loaded, "boost") == 42, "boost survived save and load");
	t.check(field<uint8_t>(loaded, "spinning") == 0, "spinning survived save and load");
	t.check(field<glm::vec3>(loaded, "step") == glm::vec3(4, 5, 6), "step survived save and load");
	t.check(label_of(loaded) == "saved label", "label survived save and load");
	t.check(field<int64_t>(loaded, "ticks") == 0, "an unchanged field keeps its initializer");

	FileSys::delete_game_file(kPath);
}
EDITOR_TEST("luasharp/editor_save_reload", 30.f, test_luasharp_editor_save_reload);
