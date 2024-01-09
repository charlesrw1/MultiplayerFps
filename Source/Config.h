#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>

using std::string;
using std::vector;

struct Config_Var
{
	string name;
	string value;
	bool persist = true;
	int integer = 0;
	float real = 0.0;
};

typedef void(*Engine_Cmd_Function)();

struct Engine_Cmd
{
	string name;
	Engine_Cmd_Function cmd;
};

class Engine_Config
{
public:
	void set_var(const char* name, const char* value);
	Config_Var* get_var(const char* name, const char* init_value, bool persist = true);
	Config_Var* find_var(const char* name);
	void set_command(const char* name, Engine_Cmd_Function cmd);

	void write_to_disk(const char* path);

	void execute(string cmd);
	void execute_file(const char* path);

	// called by Engine_Cmd_Function callbacks
	const vector<string>& get_arg_list() { return args; }
	void print_vars() {
		for (int i = 0; i < num_vars; i++) {
			printf("%--36s %s\n", vars[i].name.c_str(), vars[i].value.c_str());
		}
	}

	bool set_unknown_variables = false;	
private:
	Engine_Cmd* find_cmd(const char* name);

	static const int MAX_VARS = 256;
	static const int MAX_CMDS = 64;
	Engine_Cmd cmds[MAX_CMDS];
	int num_cmds = 0;
	Config_Var vars[MAX_VARS];
	int num_vars = 0;

	vector<string> args;
};

extern Engine_Config cfg;
#endif // !CONFIG_H