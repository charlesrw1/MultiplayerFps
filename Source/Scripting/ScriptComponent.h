#pragma once
#include "Game/EntityComponent.h"

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

CLASS_H(ScriptComponent, EntityComponent)
public:
	ScriptComponent();
	~ScriptComponent();
	void pre_start() override;
	void start() override;
	void update() override;
	void end() override;

	static const PropertyInfoList* get_props() {
		START_PROPS(ScriptComponent)
			REG_ASSET_PTR(script,PROP_DEFAULT)
		END_PROPS(ScriptComponent)
	}

	lua_State* state = nullptr;
	AssetPtr<Script> script;
};