#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"

#include "ScriptAsset.h"
#include <memory>
#include "Game/SerializePtrHelpers.h"
#include <vector>

struct lua_State;
class Script;
class ScriptComponent;
class EngineWrapper;
class ScriptManager
{
public:
	ScriptManager();
	static ScriptManager& get();
	void push_global(ClassBase* class_, const char* str);
	void remove_global(const char* str);
	void init_new_script(ScriptComponent* script, lua_State* state);	// pushes global nametable
private:
	std::unique_ptr<EngineWrapper> enginewrapper = nullptr;
};

struct PropertyInfo;
struct OutstandingScriptDelegate {
	uint64_t handle = 0;
	ClassBase* ptr = nullptr;	
	PropertyInfo* pi = nullptr;
};
CLASS_H(ScriptComponent, EntityComponent)
public:
	ScriptComponent();
	~ScriptComponent();
	void pre_start() override;
	void start() override;
	void update() override;
	void end() override;
	void editor_on_change_property() override;

	Entity* get_ref(int index) {
		if (index >= 0 && index < refs.size())
			return refs[index].get();
		sys_print(Warning, "attempted out of bounds get_ref %d %s\n", index,script->get_name().c_str());
		return nullptr;
	}
	int get_num_refs() {
		return (int)refs.size();
	}

	static const PropertyInfoList* get_props();

	std::vector<EntityPtr> refs;
	std::vector<OutstandingScriptDelegate> outstandings;
	AssetPtr<Script> script;

	lua_State* state = nullptr;
private:
	void init_vars_from_loading();
};