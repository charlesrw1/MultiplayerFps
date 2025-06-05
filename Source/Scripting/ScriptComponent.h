#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"

#include "ScriptAsset.h"
#include "Game/SerializePtrHelpers.h"
#include <vector>

struct lua_State;

struct PropertyInfo;
struct OutstandingScriptDelegate {
	uint64_t handle = 0;
	ClassBase* ptr = nullptr;	
	PropertyInfo* pi = nullptr;
};


class ScriptComponent : public Component
{
public:
	CLASS_BODY(ScriptComponent);

	ScriptComponent();
	~ScriptComponent();
	void pre_start() override;
	void start() override;
	void update() override;
	void end() override;
	void editor_on_change_property() override;

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final {
		return "eng/editor/script_lua.png";
	}
#endif

	Entity* get_ref(int index) {
		if (index >= 0 && index < refs.size())
			return refs[index].get();
		sys_print(Warning, "attempted out of bounds get_ref %d %s\n", index,script->get_name().c_str());
		return nullptr;
	}
	int get_num_refs() {
		return (int)refs.size();
	}

	bool call_function(const char* name);
	bool call_function_part1(const char* name);	// call this first, push arguments, then call part 2
	bool call_function_part2(const char* name, int num_args);
	bool has_function(const char* name);

	std::vector<EntityPtr> refs;
	std::vector<OutstandingScriptDelegate> outstandings;
	REF AssetPtr<Script> script;

	REFLECT(type = "code_block");
	std::string ctor;
private:

	void print_my_table();
	void push_table_to_stack();

	bool loaded_successfully = false;
	void init_vars_from_loading();
};