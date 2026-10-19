// Source/IntegrationTests/Tests/Scripting/test_luasharp_reload.cpp
//
// LuaSharp hot reload: the editor's file watcher sees a changed .ls file, and live components run the new code.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Framework/ClassBase.h"
#include "Framework/Files.h"
#include "Framework/ReflectionProp.h"
#include "Scripting/LuaSharpEngine.h"
#ifdef EDITOR_BUILD
#include "Assets/AssetRegistryLocal.h"
#endif
#include <string>

#ifdef EDITOR_BUILD
namespace {

constexpr const char* kPath = "scripts/tests/_temp_luasharp_reload.ls";
constexpr const char* kModule = "scripts";

bool write_game_file(const std::string& path, const std::string& content) {
	auto f = FileSys::open_write_game(path);
	if (!f)
		return false;
	f->write(content.data(), content.size());
	f->close();
	return true;
}

std::string probe_source(int step) {
	return "class LsReloadProbe : Component {\n"
		   "    [Editable] int ticks = 0;\n"
		   "    override void start() { set_ticking(true); }\n"
		   "    override void update() { ticks = ticks + " + std::to_string(step) + "; }\n"
		   "}\n";
}

const luasharp::NativeModule* current_module() { return LuaSharpEngine::inst->find_module(kModule); }

int64_t ticks_of(ClassBase* object) {
	const ClassTypeInfo& type = object->get_type();
	for (int i = 0; type.props && i < type.props->count; i++)
		if (std::string_view(type.props->list[i].name) == "ticks")
			return type.props->list[i].get<int64_t>(object);
	ASSERT(!"LsReloadProbe has a ticks field");
	return 0;
}

bool browser_lists_component(std::string_view classname) {
	auto& registry = AssetRegistrySystem::get();
	const AssetMetadata* component_type = registry.find_type("Component-Entity");
	ASSERT(component_type);
	for (const AssetFilesystemNode* node : registry.get_linear_list())
		if (node->asset.type == component_type && node->asset.filename == classname)
			return true;
	return false;
}

} // namespace

// Writes the file, then waits until the engine has a new module (the watcher debounces for 300 ms).
#define CO_AWAIT_RELOAD(t, write, what)                                                                                \
	do {                                                                                                               \
		const auto* before_ = current_module();                                                                        \
		t.require(write, what);                                                                                        \
		for (int i_ = 0; i_ < 600 && current_module() == before_; i_++)                                                \
			co_await t.wait_ticks(1);                                                                                  \
		t.require(current_module() != before_, "the engine reloaded after: " what);                                    \
	} while (0)

// Deletes the file when the test fails before it does.
struct TempFileGuard
{
	~TempFileGuard() { FileSys::delete_game_file(kPath); }
};

static TestTask test_luasharp_reload_on_save(TestContext& t) {
	t.require(LuaSharpEngine::inst && current_module(), "the scripts module is loaded");
	TempFileGuard guard;
	eng->load_level("");
	co_await t.wait_ticks(1);

	CO_AWAIT_RELOAD(t, write_game_file(kPath, probe_source(1)), "a new file");
	const ClassTypeInfo* type = ClassBase::find_class("LsReloadProbe");
	t.require(type != nullptr, "the reload registered the class of the new file");
	t.check(browser_lists_component("LsReloadProbe"), "the asset browser shows the new component");

	Entity* e = eng->get_level()->spawn_entity();
	Component* probe = e->create_component(type);
	t.require(probe != nullptr, "create_component made the probe");
	co_await t.wait_ticks(3);
	const int64_t slow = ticks_of(probe);
	t.check(slow >= 2 && slow < 100, "update() adds 1");

	CO_AWAIT_RELOAD(t, write_game_file(kPath, probe_source(100)), "a changed method");
	t.check(&probe->get_type() == type && ClassBase::find_class("LsReloadProbe") == type, "the class keeps its type");
	const int64_t before = ticks_of(probe);
	co_await t.wait_ticks(2);
	const int64_t added = ticks_of(probe) - before;
	t.check(added >= 100 && added % 100 == 0, "the live component runs the new update()");

	// A removed class must have no objects, so the last reload can remove it.
	e->destroy();
	co_await t.wait_ticks(2);
	LuaSharpEngine::inst->collect();
	CO_AWAIT_RELOAD(t, FileSys::delete_game_file(kPath), "the file was deleted");
	t.check(ClassBase::find_class("LsReloadProbe") == nullptr, "the reload removed the class of the deleted file");
	t.check(!browser_lists_component("LsReloadProbe"), "the asset browser no longer shows the removed component");
}
GAME_TEST("luasharp/reload_on_save", 30.f, test_luasharp_reload_on_save);
#endif
