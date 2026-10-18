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
#include "Assets/AssetDatabase.h"
#include "Scripting/LuaSharpEngine.h"
#include "Game/Components/PhysicsComponents.h"
#include "../LuaSharp/src/NativeRuntime.h"
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

constexpr const char* kModule = "scripts"; // all script files are one module

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

const PropertyInfo& script_prop(ClassBase* object, std::string_view name) {
	const PropertyInfo* prop = find_prop(&object->get_type(), name);
	ASSERT(prop && (prop->flags & PROP_SCRIPT_FIELD));
	return *prop;
}
// Script fields are values, read and written as the property editor and the serializer do.
template <typename T> T get_field(ClassBase* object, std::string_view name) { return script_prop(object, name).get<T>(object); }
template <typename T> void set_field(ClassBase* object, std::string_view name, const T& value) {
	script_prop(object, name).set(object, value);
}

std::string label_of(ClassBase* object) { return get_field<std::string>(object, "label"); }

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
	t.check(spinner->props && spinner->props->count == 10, "LsTestSpinner has 10 [Editable] fields");
	// Extern-class fields are asset and entity references; attributes name the editor and the former name.
	const PropertyInfo* icon = find_prop(spinner, "icon");
	t.require(icon && icon->type == core_type_id::AssetPtr, "a Texture field is an asset property");
	t.check(icon->class_type == ClassBase::find_class("Texture"), "the asset property accepts textures");
	const PropertyInfo* target = find_prop(spinner, "target");
	t.require(target && target->type == core_type_id::ObjHandlePtr, "an Entity field is an entity reference");
	t.check(std::string_view(find_prop(spinner, "bone")->custom_type_str) == "BoneNameString", "[Editor] sets the editor");
	const PropertyInfo* count = find_prop(spinner, "count");
	t.check(count->former_name && std::string_view(count->former_name) == "old_count", "[Former] sets the former name");
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
	t.check(get_field<double>(comp, "speed") == 2.0, "the field initializer ran");

	// update(): ticked by the level after set_ticking(true) in the script.
	set_field(comp, "speed", 0.5); // the same write the property editor does
	co_await t.wait_ticks(5);
	const int64_t ticks = get_field<int64_t>(comp, "ticks");
	t.check(ticks >= 4, "update() override ran each frame");
	t.check(e->get_ws_position().x == static_cast<float>(ticks) * 0.5f, "update() moved the entity by step * speed");

	// A virtual call from C++ goes to the script override too.
	comp->update();
	t.check(get_field<int64_t>(comp, "ticks") == ticks + 1, "a C++ virtual call ran the script override");

	// An Entity field: the script sees the entity that C++ stored, and C++ reads it back as an EntityRef.
	Entity* other = eng->get_level()->spawn_entity();
	set_field(comp, "target", EntityRef{other->get_instance_id()});
	t.check(get_field<EntityRef>(comp, "target").handle == other->get_instance_id(), "the Entity field reads back");
	co_await t.wait_ticks(2);
	t.check(get_field<bool>(comp, "sees_target"), "the script reads the Entity that C++ wrote");
	other->destroy();
	co_await t.wait_ticks(2);
	t.check(get_field<EntityRef>(comp, "target").handle == 0, "a destroyed entity reads as null");

	set_field(comp, "spinning", false);
	const int64_t frozen = get_field<int64_t>(comp, "ticks");
	co_await t.wait_ticks(3);
	t.check(get_field<int64_t>(comp, "ticks") == frozen, "the script reads fields that C++ wrote");

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

	set_field(comp, "speed", 1.0);
	set_field(comp, "boost", int64_t(10));
	const int64_t before = get_field<int64_t>(comp, "ticks");
	comp->update();
	t.check(get_field<int64_t>(comp, "ticks") == before + 11, "the override called base.update() and added boost");
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

	set_field(comp, "speed", 0.25);
	set_field(comp, "boost", int64_t(42));
	set_field(comp, "spinning", false);
	set_field(comp, "step", glm::vec3(4, 5, 6));
	set_field(comp, "label", std::string("saved label"));
	set_field(comp, "bone", std::string("spine"));

#ifdef EDITOR_BUILD
	{ // the editors refer to the EditorDoc, which reopening the level destroys
		// The property grid makes a row for each [Editable] field.
		FnFactory<IPropertyEditor> factory;
		PropertyFactoryUtil::register_basic(factory);
		PropertyFactoryUtil::register_editor(*doc, factory);
		PropertyGrid grid(factory);
		grid.add_class_to_grid(comp);
		int script_rows = 0;
		for (auto& row : grid.rows)
			if (auto* group = dynamic_cast<GroupRow*>(row.get()))
				if (group->proplist == type->props || group->proplist == type->super_typeinfo->props)
					script_rows += static_cast<int>(group->child_rows.size());
		t.check(script_rows == 11, "the property grid shows the 11 [Editable] fields");

		// The C++ editors edit the script fields: the asset picker and the entity picker.
		PropertyInfo* icon = const_cast<PropertyInfo*>(find_prop(type, "icon"));
		auto icon_editor = create_ipropertyed(factory, PropertyRef{comp, PropertyPath(icon)}, nullptr);
		auto* asset_editor = dynamic_cast<AssetPropertyEditor*>(icon_editor.get());
		t.require(asset_editor != nullptr, "a Texture field gets the asset editor");
		asset_editor->set_asset("eng/icons/undo.png");
		t.check(asset_editor->get_str() == "eng/icons/undo.png", "the asset editor reads the script field");
		t.check(get_field<IAsset*>(comp, "icon") != nullptr, "the asset editor wrote the script field");

		PropertyInfo* target = const_cast<PropertyInfo*>(find_prop(type, "target"));
		auto target_editor = create_ipropertyed(factory, PropertyRef{comp, PropertyPath(target)}, nullptr);
		auto* entity_editor = dynamic_cast<EntityTargetEditor*>(target_editor.get());
		t.require(entity_editor != nullptr, "an Entity field gets the entity editor");
		Entity* picked = doc->spawn_entity();
		entity_editor->set_target(picked);
		t.check(get_field<EntityRef>(comp, "target").handle == picked->get_instance_id(), "the entity editor wrote the field");
		t.check(entity_editor->find_target() == picked, "the entity editor reads the field");

		PropertyInfo* bone = const_cast<PropertyInfo*>(find_prop(type, "bone"));
		t.check(dynamic_cast<BoneNameStringEditor*>(create_ipropertyed(factory, PropertyRef{comp, PropertyPath(bone)}, nullptr).get()) != nullptr,
				"[Editor(\"BoneNameString\")] picks the bone name editor");
	}
#else
	set_field(comp, "icon", g_assets.generic_find("eng/icons/undo.png", ClassBase::find_class("Texture")).get());
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
	t.check(get_field<double>(loaded, "speed") == 0.25, "speed survived save and load");
	t.check(get_field<int64_t>(loaded, "boost") == 42, "boost survived save and load");
	t.check(!get_field<bool>(loaded, "spinning"), "spinning survived save and load");
	t.check(get_field<glm::vec3>(loaded, "step") == glm::vec3(4, 5, 6), "step survived save and load");
	t.check(label_of(loaded) == "saved label", "label survived save and load");
	t.check(get_field<std::string>(loaded, "bone") == "spine", "bone survived save and load");
	IAsset* loaded_icon = get_field<IAsset*>(loaded, "icon");
	t.check(loaded_icon && loaded_icon->get_name() == "eng/icons/undo.png", "the Texture field saved as its path");
	t.check(get_field<int64_t>(loaded, "ticks") == 0, "an unchanged field keeps its initializer");

	FileSys::delete_game_file(kPath);
}
EDITOR_TEST("luasharp/editor_save_reload", 30.f, test_luasharp_editor_save_reload);

// A STRUCT_BODY struct crosses by value: cast_ray returns a HitResult whose obj<Entity> field is the proxy of the entity.
static TestTask test_luasharp_struct_hit_result(TestContext& t) {
	t.require(LuaSharpEngine::inst != nullptr, "LuaSharpEngine is created at startup");
	eng->load_level("");
	Entity* box = eng->get_level()->spawn_entity();
	box->set_ws_position(glm::vec3(0.f));
	box->create_component<BoxComponent>();
	co_await t.wait_ticks(1);

	const luasharp::NativeModule module = LuaSharpEngine::inst->compile(R"(
HitResult hit = GameplayStatic.cast_ray(Vec3.new(0, 10, 0), Vec3.new(0, -10, 0), -1, null);
bool didHit = hit.hit;
float hitY = hit.pos.y;
float normalY = hit.normal.y;
Entity what = hit.what;
HitResult miss = GameplayStatic.cast_ray(Vec3.new(100, 10, 0), Vec3.new(100, -10, 0), -1, null);
bool missed = !miss.hit && miss.what == null;
lColor color = lColor();
color.g = 255;
GameplayStatic.debug_sphere(hit.pos, 0.5, 0.1, color);
)");
	const auto global = [&]<typename T>(const char* name, T) {
		const auto* value = static_cast<const T*>(module.FindGlobal(name));
		ASSERT(value);
		return *value;
	};
	t.check(module.LastError().empty(), "the script ran without a runtime error");
	t.check(global("didHit", uint8_t{}) == 1, "the ray hit the box");
	const double hit_y = global("hitY", double{});
	t.check(hit_y > 0.0 && hit_y < 10.0, "the hit position is on top of the box");
	t.check(global("normalY", double{}) > 0.9, "the hit normal points up");
	t.check(luasharp::NativeRuntime::NativeOf(global("what", static_cast<void*>(nullptr))) == static_cast<ClassBase*>(box),
			"HitResult.what is the proxy of the hit entity");
	t.check(global("missed", uint8_t{}) == 1, "a miss has hit false and a null entity");
	co_return;
}
GAME_TEST("luasharp/struct_hit_result", 10.f, test_luasharp_struct_hit_result);

// A std::vector<obj<Entity>> result crosses as a copy in an Entity[] list.
static TestTask test_luasharp_list_overlap(TestContext& t) {
	eng->load_level("");
	Entity* box = eng->get_level()->spawn_entity();
	box->set_ws_position(glm::vec3(0.f));
	box->create_component<BoxComponent>();
	co_await t.wait_ticks(1);

	const luasharp::NativeModule module = LuaSharpEngine::inst->compile(R"(
Entity[] near = GameplayStatic.sphere_overlap(Vec3.new(0, 0, 0), 2.0, -1);
int nearCount = near.size();
Entity first = null;
if (nearCount > 0) { first = near[0]; }
int farCount = GameplayStatic.sphere_overlap(Vec3.new(100, 0, 0), 1.0, -1).size();
)");
	const auto global = [&]<typename T>(const char* name, T) {
		const auto* value = static_cast<const T*>(module.FindGlobal(name));
		ASSERT(value);
		return *value;
	};
	t.check(module.LastError().empty(), "the script ran without a runtime error");
	t.check(global("nearCount", int64_t{}) == 1, "the sphere overlaps the box");
	t.check(luasharp::NativeRuntime::NativeOf(global("first", static_cast<void*>(nullptr))) == static_cast<ClassBase*>(box),
			"the list element is the proxy of the box entity");
	t.check(global("farCount", int64_t{}) == 0, "a sphere away from the box overlaps nothing");
	co_return;
}
GAME_TEST("luasharp/list_overlap", 10.f, test_luasharp_list_overlap);
