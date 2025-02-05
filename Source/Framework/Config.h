#ifndef CONFIG_H
#define CONFIG_H
#include "Framework/Util.h"

enum CVarFlags
{
	CVAR_READONLY	= 1,
	CVAR_SAVETODISK	= (1 << 1),
	CVAR_INTEGER	= (1 << 3),
	CVAR_BOOL		= (1 << 4),
	CVAR_FLOAT		= (1 << 5),
	CVAR_REGISTERED	= (1 << 6),
	CVAR_DEV		= (1 << 7),
	CVAR_UNBOUNDED	= (1 << 8),
	// used internally
	CVAR_CHANGED	= (1<<9),
};


class ConfigVarDataPublic
{
public:
	const char* name = "";
	const char* value = "";
	const char* description = "";
	uint32_t flags = 0;	// CVarFlags
	float minVal = 0.f;
	float maxVal = 1.f;
	int integerVal = 0;
	float floatVal = 0.0;
};

class ConfigVarDataInternal;
class ConfigVar
{
public:
	ConfigVar(const char* name, const char* value, int flags, const char* description, float min = -1.f, float max = 1.f);

	int get_integer() const { return ptr->integerVal; }
	float get_float() const { return ptr->floatVal; }
	bool get_bool() const { return ptr->integerVal; }
	const char* get_string() const { return ptr->value; }
	int get_var_flags() const { return ptr->flags; }
	const char* get_name() const { return ptr->name; }

	// this resets the flag too
	bool was_changed() {
		bool b = bool(ptr->flags & CVAR_CHANGED);
		ptr->flags &= ~CVAR_CHANGED;
		return b;
	}
	void force_set_has_changed() {
		ptr->flags |= CVAR_CHANGED;
	}

	void set_bool(bool setI);
	void set_integer(int setI);
	void set_float(float setF);
	void set_string(const char* str);
private:
	ConfigVar(ConfigVarDataInternal* ptr);
	ConfigVarDataPublic* ptr = nullptr;

	friend class ConfigVarDataInternal;
	friend class VarManImpl;
};


class VarMan
{
public:
	static VarMan* get();
	virtual void register_var(ConfigVar* var, ConfigVarDataPublic initializer) = 0;
	virtual ConfigVar* find(const char* name) = 0;
	virtual void set_var_string(const char* name, const char* value) =0 ;
	virtual void set_var_int(const char* name,int iVAl) = 0;
	virtual void set_var_bool(const char* name, bool bVal) = 0;
	virtual void set_var_float(const char* name, float fVal) = 0;
};

class Cmd_Manager_Impl;
class Cmd_Args
{
public:
	int size() const {
		return argc;
	}
	const char* at(int index) const;

	void add_arg(const char* v, int len);
	void clear() {
		argc = 0;
		buffer_index = 0;
	}
private:
	static const int MAX_ARGS = 8;
	static const int BUFFER_SIZE = 150;
	int argc = 0;
	int arg_to_index[MAX_ARGS];
	int buffer_index = 0;
	char buffer[BUFFER_SIZE];

};

typedef void(*Engine_Cmd_Function)(const Cmd_Args& args);

enum class Cmd_Execute_Mode
{
	NOW,
	APPEND,
};

class Cmd_Manager
{
public:
	static Cmd_Manager* get();
	virtual void add_command(const char* name, Engine_Cmd_Function) = 0;
	virtual void execute(Cmd_Execute_Mode mode, const char* command_string) = 0;
	virtual void execute_file(Cmd_Execute_Mode mode, const char* path) = 0;
	virtual void execute_buffer() = 0;
	virtual void set_set_unknown_variables(bool b) = 0;
	// returns string of match if one match exists, otherwise null
	virtual const char* print_matches(const char* match) = 0;
};

class Auto_Engine_Cmd
{
public:
	Auto_Engine_Cmd(const char* name, Engine_Cmd_Function cmd) {
		Cmd_Manager::get()->add_command(name, cmd);
	}
};

class Debug_Interface
{
public:
	static Debug_Interface* get();

	virtual void add_hook(const char* menu_name, void(*drawfunc)()) = 0;
	virtual void draw() = 0;
};

struct AddToDebugMenu
{
	AddToDebugMenu(const char* name, void(*func)()) {
		Debug_Interface::get()->add_hook(name, func);
	}
};

#define ADD_TO_DEBUG_MENU(funcname) static AddToDebugMenu debugmenuadd##funcname(#funcname, funcname);

#define DECLARE_ENGINE_CMD(func_name) static void enginecmd_##func_name(const Cmd_Args&); static Auto_Engine_Cmd autoenginecmd_##func_name(#func_name, enginecmd_##func_name); static void enginecmd_##func_name(const Cmd_Args& args)
#define DECLARE_ENGINE_CMD_CAT(category, func_name) static void enginecmd_##func_name(const Cmd_Args&); static Auto_Engine_Cmd autoenginecmd_##func_name(category#func_name, enginecmd_##func_name); static void enginecmd_##func_name(const Cmd_Args& args)

#endif // !CONFIG_H