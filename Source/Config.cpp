#include "Config.h"
#include <fstream>

Config_Var* Engine_Config::get_var(const char* name, const char* value, bool persist)
{
	Config_Var* var = find_var(name);
	if (var) {
		return var;
	}

	if (num_vars >= MAX_VARS) {
		printf("Engine_Config::get_var Max Vars Reached\n");
		return nullptr;
	}
	var = &vars[num_vars++];

	var->name = name;
	var->value = value;
	var->persist = persist;
	var->integer = std::atoi(value);
	var->real = std::atof(value);

	return var;
}
void Engine_Config::set_var(const char* name, const char* value)
{
	Config_Var* var = find_var(name);
	if (var) {
		var->value = value;
		var->integer = std::atoi(value);
		var->real = std::atof(value);
		return;
	}
	var = get_var(name, value, true);
}

void Engine_Config::set_command(const char* name, Engine_Cmd_Function cmd)
{
	if (find_cmd(name)) {
		printf("Engine_Config::set_command Duplicate command %s\n", name);
		return;
	}
	if (num_cmds >= MAX_CMDS) {
		printf("Engine_Config::set_command Max commands reached\n");
		return;
	}
	Engine_Cmd* command = &cmds[num_cmds++];
	command->cmd = cmd;
	command->name = name;
}

void tokenize_string(string& input, std::vector<string>& out)
{
	string token;
	bool in_quotes = false;
	for (char c : input) {
		if (c == ' ' && !in_quotes) {
			if (!token.empty()) {
				out.push_back(token);
				token.clear();
			}
		}
		else if (c == '"') {
			in_quotes = !in_quotes;
			if (!token.empty()) {
				out.push_back(token);
				token.clear();
			}
		}
		else {
			token += c;
		}
	}
	if (!token.empty()) {
		out.push_back(token);
		token.clear();
	}
}

void Engine_Config::execute(string command)
{
	args.clear();
	tokenize_string(command, args);
	if (args.size() == 0) return;
	Engine_Cmd* ec = find_cmd(args[0].c_str());
	if (ec) {
		ec->cmd();
	}
	else {
		Config_Var* var = find_var(args[0].c_str());
		if (var && args.size() == 1)
			printf("%s %s\n", var->name, var->value);
		else if (!var && args.size() == 1)
			printf(__FUNCTION__": no Config_Var for %s\n", args[0].c_str());
		else
			set_var(args[0].c_str(), args[1].c_str());
	}
}

void Engine_Config::execute_file(const char* filepath)
{
	std::ifstream infile(filepath);
	if (!infile) {
		printf(__FUNCTION__": couldn't open file\n");
		return;
	}
	std::string line;
	while (std::getline(infile, line)) {
		if (line.empty())
			continue;
		if (line.at(0) == '#')
			continue;
		execute(line);
	}
}

Config_Var* Engine_Config::find_var(const char* name)
{
	for (int i = 0; i < num_vars; i++) {
		if (vars[i].name == name) return &vars[i];
	}
	return nullptr;
}
Engine_Cmd* Engine_Config::find_cmd(const char* name)
{
	for (int i = 0; i < num_cmds; i++) {
		if (cmds[i].name == name) return &cmds[i];
	}
	return nullptr;
}