#include "Framework/Config.h"
#include "Framework/Util.h"
#include <fstream>
#include <vector>
#include <unordered_map>

#include "imgui.h"
#include "Framework/Files.h"
#include "Framework/BinaryReadWrite.h"
#include "Framework/DictParser.h"
class Var_Manager_Impl : public Var_Manager
{
public:
	Var_Manager_Impl();

	Config_Var* create_var(const char* name, const char* description, Generic_Value default_val, int flags) {
		Config_Var* var = get_var(name);
		if (var) {
			if (var->flags & (int)CVar_Flags::SETFROMDISK) {
				var->description = description;
				var->flags |= flags;

				var->flags &= ~(int)CVar_Flags::SETFROMDISK;
			}
			else {
				sys_print("Duplicate call to create var for %s\n", name);
			}
			return var;
		}
		ASSERT(num_vars < 512);
		var = &all_vars[num_vars++];
		var->name = name;
		var->description = description;
		var->flags = flags;
		should_sort_vars = true;
		var_remap[num_vars - 1] = num_vars - 1;
		hash_to_index[StringUtils::StringHash(name).computedHash] = num_vars - 1;

		set_var(var, default_val);
		return var;
	}
	Config_Var* set_var(StringUtils::StringHash hash, Generic_Value value) {
		Config_Var* var = get_var(hash);
		if (!var)
			return nullptr;
		set_var(var, value);
		return var;
	}

	Config_Var* get_all_var_list(int* length) {
		*length = num_vars;
		return all_vars;
	}
	void set_var(Config_Var* var, Generic_Value value) {
		if (var->flags & (int)CVar_Flags::READONLY) {
			sys_print("Attempt to write to readonly var %s\n", var->name.c_str());
			return;
		}
		var->type = value.type;
		switch (var->type) {
		case Generic_Value::STRING:
			strcpy(var->string_val, value.string_val);
			break;
		case Generic_Value::INT:
			var->integer = value.integer;
			break;
		case Generic_Value::FLOAT:
			var->real = value.real;
			break;
		}
	}

	Config_Var* get_var(StringUtils::StringHash hash) {
		auto f = hash_to_index.find(hash.computedHash);
		if (f != hash_to_index.end()) {
			return &all_vars[f->second];
		}
		return nullptr;
	}
	void print_vars(const char* match) {
		sys_print("%--36s %s", "name", "value");
		//for (int i = 0; i < num_vars; i++)
			//if (!match || all_vars[i].name.find(match) != std::string::npos)
				//sys_print("%--36s %s\n", all_vars[i].name.c_str(), all_vars[i].value.c_str());
	}

	void imgui_draw() {
		if (should_sort_vars) {
			sort_vars();
			should_sort_vars = false;
		}

		for (int i = 0; i < num_vars; i++) {
			Config_Var* var = all_vars + (var_remap[i]);
			bool readonly = var->flags & (int)CVar_Flags::READONLY;

			if (var->type == Generic_Value::INT) {
				if (var->flags & (int)CVar_Flags::INTEGER) {
					ImGui::SliderInt(var->name.c_str(), &var->integer, 0, 5);
				}
				else {
					bool b = var->integer;
					if (ImGui::Checkbox(var->name.c_str(), &b)) {
						var->integer = b;
					}
				}
			}
			else if (var->type == Generic_Value::STRING) {
				ImGui::InputText(var->name.c_str(), var->string_val, 64);
			}
			else if (var->type == Generic_Value::FLOAT) {
				ImGui::DragFloat(var->name.c_str(), &var->real, 0.01);
			}
		}
	}

	int var_type(Config_Var* var) {
		if (var->type == Generic_Value::INT) {
			if (var->flags & (int)CVar_Flags::INTEGER)
				return 0;
			else
				return 1;
		}
		else if (var->type == Generic_Value::STRING)
			return 2;
		else if (var->type == Generic_Value::FLOAT)
			return 3;
	}

	void sort_vars() {
	
	}

	bool should_sort_vars = false;
	bool create_var_on_unknown = false;
	int num_vars = 0;
	Config_Var all_vars[512];
	int var_remap[512];
	std::unordered_map<uint32_t, int> hash_to_index;
};

Var_Manager* Var_Manager::get()
{
	static Var_Manager_Impl inst;
	return &inst;
}

void imgui_vars_hook()
{
	((Var_Manager_Impl*)Var_Manager::get())->imgui_draw();
}

Var_Manager_Impl::Var_Manager_Impl()
{
	Debug_Interface::get()->add_hook("Cvars", imgui_vars_hook);
}


void tokenize_string(std::string& input, Cmd_Args& output)
{
	std::string token;
	bool in_quotes = false;
	for (char c : input) {
		if ((c == ' ' || c == '\t' || c=='\r') && !in_quotes) {
			if (!token.empty()) {
				output.add_arg(token.c_str(), token.size());
				token.clear();
			}
		}
		else if (c == '"') {
			in_quotes = !in_quotes;
			if (!token.empty()) {
				output.add_arg(token.c_str(),token.size());
				token.clear();
			}
		}
		else {
			token += c;
		}
	}
	if (!token.empty()) {
		output.add_arg(token.c_str(), token.size());
		token.clear();
	}
}

// shitty classification
static int classify_string(const char* s)
{
	int l = strlen(s);
	bool floating_point = false;
	for (int i = 0; i < l; i++) {
		if (s[i] == '.') {
			floating_point = true;
			continue;
		}
		if (s[i] == 'f' && i == l - 1) {
			floating_point = true;
		}
		if (s[i] >= '0' && s[i] <= '9') continue;

		return 0;
	}
	return (floating_point) ? 2 : 1;
}

Generic_Value to_gv(const char* s)
{
	int c = classify_string(s);
	if (c == 0) return Generic_Value(s);
	if (c == 1) return Generic_Value(atoi(s));
	if (c == 2) return Generic_Value((float)atof(s));
}





class Cmd_Manager_Impl : public Cmd_Manager
{
public:
	void add_command(const char* name, Engine_Cmd_Function cmd) {
		StringUtils::StringHash hash(name);

		if (find_cmd(hash)) {
			printf("Set duplicate command %s\n", name);
			return;
		}
		if (num_cmds >= MAX_CMDS) {
			printf("Max commands reached\n");
			return;
		}
		Engine_Cmd* command = &all_cmds[num_cmds++];
		command->func = cmd;
		command->name = name;
		hash_to_index[hash.computedHash] = num_cmds - 1;
	}
	void execute(Cmd_Execute_Mode mode, const char* command_string) {
		sys_print("#%s\n", command_string);
		Cmd_Args args;
		std::string command = command_string;
		tokenize_string(command, args);
		if (args.size() == 0) return;
		Engine_Cmd* ec = find_cmd(args.at(0));
		if (ec) {
			ec->func(args);
		}
		else {
			Config_Var* var = Var_Manager::get()->get_var(args.at(0));
			if (var && args.size() == 1) {
				sys_print("%s %s\n", var->name.c_str(), "XYZ");
			}

			else if (!var && set_unknown_variables && args.size() == 2)
				Var_Manager::get()->create_var(args.at(0), "", to_gv(args.at(1)), (int)CVar_Flags::SETFROMDISK);
			else if (var && args.size() == 2)
				Var_Manager::get()->set_var(args.at(0), to_gv(args.at(1)));
			else
				sys_print("unknown command: %s\n",args.at(0));
		}
	}
	void execute_file(Cmd_Execute_Mode mode, const char* path) {

		auto file = FileSys::open_read_os(path);
		if(!file) {
			sys_print("!!! couldn't open config file to execute: %s\n", path);
			return;
		}
		
		DictParser parser;
		parser.load_from_file(file.get());

		StringView view;
		while (parser.read_line(view)) {
			if (view.is_empty())
				continue;
			if (view.str_start[0] == '#')
				continue;

			std::string str = std::string(view.str_start, view.str_len);
			execute(Cmd_Execute_Mode::NOW, str.c_str());
		}
	}
	void execute_buffer() {

	}

	void set_set_unknown_variables(bool b) override {
		set_unknown_variables = b;
	}




	struct Engine_Cmd {
		std::string name;
		Engine_Cmd_Function func;
	};

	Engine_Cmd* find_cmd(StringUtils::StringHash hash) {
		auto find = hash_to_index.find(hash.computedHash);
		if (find != hash_to_index.end()) return all_cmds + find->second;
		return nullptr;
	}
	
	bool set_unknown_variables = false;
	static const int MAX_CMDS = 64;
	int num_cmds = 0;
	Engine_Cmd all_cmds[MAX_CMDS];
	std::unordered_map<uint32_t, int> hash_to_index;
};

Cmd_Manager* Cmd_Manager::get()
{
	static Cmd_Manager_Impl inst;
	return &inst;
}

inline const char* Cmd_Args::at(int index) const {
	ASSERT(index >= 0 && index < argc&& index < MAX_ARGS);
	return &buffer[arg_to_index[index]];
}

inline void Cmd_Args::add_arg(const char* v, int len) {
	if (len + 1 + buffer_index >= BUFFER_SIZE || argc >= MAX_ARGS) return;
	memcpy(buffer + buffer_index, v, len);
	buffer[buffer_index + len] = 0;
	arg_to_index[argc] = buffer_index;
	buffer_index += len + 1;
	argc++;
}
