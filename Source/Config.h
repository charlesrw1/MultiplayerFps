#ifndef CONFIG_H
#define CONFIG_H
#include "Util.h"
#include "StringUtil.h"

struct Generic_Value
{
	enum Type { STRING, INT, FLOAT } type;
	union {
		char string_val[64];
		int integer;
		float real;
	};
	Generic_Value() {
		integer = 0;
		type = INT;
	}
	Generic_Value(const char* str) { 
		int len = strlen(str);
		if (len < 63) {
			type = STRING;
			memcpy_s(string_val, 63, str,len);
			string_val[len] = 0;
		}
	}
	Generic_Value(int i) { type = INT; integer = i; }
	Generic_Value(float f) { type = FLOAT; real = f; }
};

struct Config_Var : Generic_Value
{
	Stack_String<64> name;
	Stack_String<64> description;

	int flags = 0;
	bool persist = true;
};


enum class CVar_Flags
{
	READONLY = 1,
	SAVETODISK = (1<<1),

	// used for imgui displaying
	INTEGER = (1<<2),	// otherwise treat as boolean if integer type

	// internal flag
	SETFROMDISK = (1<<3)
};


class Var_Manager
{
public:
	static Var_Manager* get();
	virtual Config_Var* create_var(const char* name, const char* description, Generic_Value value, int flags) = 0;
	virtual Config_Var* get_var(StringUtils::StringHash hash) = 0;
	virtual Config_Var* set_var(StringUtils::StringHash hash, Generic_Value value) = 0;
	virtual void set_var(Config_Var* var, Generic_Value value) = 0;
	virtual void print_vars(const char* match) = 0;
	virtual Config_Var* get_all_var_list(int* length) = 0;
};

class Auto_Config_Var
{
public:
	Auto_Config_Var(const char* name, Generic_Value value, int flags = 0, const char* desc = "") {
		var = Var_Manager::get()->create_var(name, desc, value, flags);
	}

	int& integer() { return var->integer; }
	float& real() { return var->real; }
	const char* cstring() { return var->string_val; }

	Config_Var* var = nullptr;
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


#define DECLARE_ENGINE_CMD(func_name) static void enginecmd_##func_name(const Cmd_Args&); static Auto_Engine_Cmd autoenginecmd_##func_name(#func_name, enginecmd_##func_name); static void enginecmd_##func_name(const Cmd_Args& args)
#define DECLARE_ENGINE_CMD_CAT(category, func_name) static void enginecmd_##func_name(const Cmd_Args&); static Auto_Engine_Cmd autoenginecmd_##func_name(category#func_name, enginecmd_##func_name); static void enginecmd_##func_name(const Cmd_Args& args)

#endif // !CONFIG_H