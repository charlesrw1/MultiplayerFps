#include "Framework/Config.h"
#include "Framework/Util.h"
#include <fstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "imgui.h"
#include "Framework/Files.h"
#include "Framework/BinaryReadWrite.h"
#include "Framework/DictParser.h"

#include "ConsoleCmdGroup.h"

class ConfigVarDataInternal : public ConfigVarDataPublic
{
public:
	ConfigVarDataInternal(const ConfigVarDataPublic& data) : selfptr(this) {
		nameStr = data.name;
		valueStr = data.value;

		name = nameStr.c_str();
		value = valueStr.c_str();

		minVal = data.minVal;
		maxVal = data.maxVal;
		flags |= data.flags;
		description = data.description;

		update();
	}

	void init_from_register(const ConfigVarDataPublic& data) {
		valueStr = data.value;
		value = valueStr.c_str();
		minVal = data.minVal;
		maxVal = data.maxVal;
		flags |= data.flags;
		description = data.description;


		update();
	}

	void set_string(const char* value) {
		if (flags & CVAR_READONLY) {
			sys_print(Warning, "cant set readonly cvar %s, restart required to set\n", value);
			return;
		}

		valueStr = value;
		this->value = valueStr.c_str();
		update();
	}
	void update() {
		flags |= CVAR_CHANGED;
		if (flags & CVAR_BOOL) {
			integerVal = atoi(value)!=0;
			valueStr = std::to_string(integerVal);
			value = valueStr.c_str();
		}
		else if (flags & CVAR_INTEGER) {
			integerVal = atoi(value);
			if (!(flags & CVAR_UNBOUNDED)) {
				if (integerVal < minVal)integerVal = minVal;
				else if (integerVal > maxVal) integerVal = maxVal;
				valueStr = std::to_string(integerVal);
				value = valueStr.c_str();
			}
		}
		else if (flags & CVAR_FLOAT) {
			floatVal = atof(value);
			if (!(flags & CVAR_UNBOUNDED)) {
				if (floatVal < minVal)floatVal = minVal;
				else if (floatVal > maxVal) floatVal = maxVal;
				valueStr = std::to_string(floatVal);
				value = valueStr.c_str();
			}
		}
	}

	std::string nameStr;
	std::string valueStr;

	ConfigVar selfptr;

};


ConfigVar::ConfigVar(ConfigVarDataInternal* ptr)
{
	this->ptr = ptr;
}

ConfigVar::ConfigVar(const char* name, const char* value, int flags, const char* description, float min , float max )
{
	ConfigVarDataPublic init;
	init.name = name;
	init.value = value;
	init.flags = flags;
	init.minVal = min;
	init.maxVal = max;
	init.description = description;
	VarMan::get()->register_var(this, init);
}

void ConfigVar::set_string(const char* s)
{
	((ConfigVarDataInternal*)ptr)->set_string(s);
}
void ConfigVar::set_bool(bool b)
{
	set_string(std::to_string((int)b).c_str());
}

void ConfigVar::set_float(float f)
{
	set_string(std::to_string(f).c_str());
}
void ConfigVar::set_integer(int i)
{
	set_string(std::to_string(i).c_str());
}




class VarManImpl : public VarMan
{
public:
	VarManImpl();

	ConfigVar* find(const char* name) {
		auto find = vars.find(name);
		if (find != vars.end()) 
			return &find->second->selfptr;
		return nullptr;
	}

	void register_var(ConfigVar* var, ConfigVarDataPublic initializer) override {

		ConfigVarDataInternal* internal_ = find_internal_var(initializer.name);
		if (internal_) {
			// already have a internal var, update it unless we tried registering a var twice
			if (internal_->flags & CVAR_REGISTERED)
				Fatalf("config var was allocated twice %s\n", internal_->name);
			internal_->init_from_register(initializer);
		}
		else {
			auto pair = vars.insert({ std::string(initializer.name),new ConfigVarDataInternal(initializer) });
			internal_ = pair.first->second;
		}
		internal_->flags |= CVAR_REGISTERED;
		var->ptr = internal_;
	}

	void set_var_string(const char* name, const char* value) {
		auto internal_ = find_internal_var(name);
		if (internal_) {
			internal_->set_string(value);
		}
		else {
			ConfigVarDataPublic initializer;
			initializer.name = name;
			initializer.value = value;
			vars.insert({ std::string(name), new ConfigVarDataInternal(initializer) });
		}
	}
	void set_var_int(const char* name,int iVal) {
		set_var_string(name, std::to_string(iVal).c_str());
	}
	void set_var_bool(const char* name, bool bVal) {
		set_var_string(name, std::to_string((int)bVal).c_str());
	}
	void set_var_float(const char* name, float fVal) {
		set_var_string(name, std::to_string(fVal).c_str());
	}

	
	void print_vars(const char* match) {
		sys_print(Info, "%--36s %s", "name", "value");
		//for (int i = 0; i < num_vars; i++)
			//if (!match || all_vars[i].name.find(match) != std::string::npos)
				//sys_print("%--36s %s\n", all_vars[i].name.c_str(), all_vars[i].value.c_str());
	}

	void imgui_draw() {
	

		for (auto var_pair : vars) {
			auto var = var_pair.second;

			bool readonly = var->flags & CVAR_READONLY;

			if (readonly) {
				ImGui::Text(var->value);
			}
			else if (var->flags & CVAR_INTEGER) {

			}
			else if (var->flags & CVAR_FLOAT) {

			}
			else if (var->flags & CVAR_BOOL) {

			}
			else {

			}
		}
	}


	void sort_vars() {
	
	}

	ConfigVarDataInternal* find_internal_var(const std::string& name) {
		auto find = vars.find(name);
		if (find != vars.end()) 
			return find->second;
		return nullptr;
	}

	bool should_sort_vars = false;
	bool create_var_on_unknown = false;
	std::unordered_map<std::string, ConfigVarDataInternal*> vars;
};

VarMan* VarMan::get()
{
	static VarManImpl inst;
	return &inst;
}

void imgui_vars_hook()
{
	((VarManImpl*)VarMan::get())->imgui_draw();
}

VarManImpl::VarManImpl()
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


#include <algorithm>


static StringView strip_string_view(StringView sv)
{
	int i = 0;
	for (; i < sv.str_len; i++) {
		char c = sv.str_start[i];
		if (!(c == ' ' || c == '\t' || c=='\r'))
			break;
	}
	return StringView(sv.str_start + i, sv.str_len - i);
}

class Cmd_Manager_Impl : public Cmd_Manager
{
public:
	const char* print_matches(const char* match) {
		struct Match {
			const char* name = "";
			const char* desc = "";
		};
		std::vector<Match> matches;
		VarManImpl* v = (VarManImpl*)VarMan::get();
		for (auto& var : v->vars) {
			if (var.first.find(match) != std::string::npos) {
				matches.push_back({ var.first.c_str(),var.second->description });
			}
		}
		for (auto& c : all_cmds) {
			if (c.first.find(match) != std::string::npos)
				matches.push_back({ c.first.c_str(),nullptr });
		}
		if (matches.size() == 1)
			return matches[0].name;

		std::sort(matches.begin(), matches.end(), [](Match a, Match b)
			{
				return strcmp(a.name, b.name) < 0;
			});
		sys_print(Info, ">");
		for (auto m : matches) {
			if(m.desc)
				sys_print(Info, ". %-24s: %s\n", m.name,m.desc);
			else
				sys_print(Info, ". %s\n", m.name);
		}
		return nullptr;
	}
	void add_command(const char* name, Engine_Cmd_Function cmd) {

		if (find_cmd(name)) {
			sys_print(Warning, "Set duplicate command %s\n", name);
			return;
		}
		all_cmds.insert({ std::string(name),cmd });
	}
	void execute_string(const char* command_string) {

		sys_print(Info, "> %s\n", command_string);

		Cmd_Args args;
		std::string command = command_string;
		tokenize_string(command, args);
		if (args.size() == 0) return;
		Engine_Cmd_Function ec = find_cmd(args.at(0));
		if (ec) {
			ec(args);
		}
		else {
			auto group_cmd = find_cmd_in_groups(args.at(0));
			if (group_cmd) {
				(*group_cmd)(args);
			}
			else {
				ConfigVar* var = VarMan::get()->find(args.at(0));
				if (var && args.size() == 1) {
					sys_print(Info, "%s %s\n", var->get_name(), var->get_string());
				}

				else if (!var && set_unknown_variables && args.size() == 2)
					VarMan::get()->set_var_string(args.at(0), args.at(1));
				else if (var && args.size() == 2)
					var->set_string(args.at(1));
				else
					sys_print(Error, "unknown command: %s\n", args.at(0));
			}
		}
	}

	void execute(Cmd_Execute_Mode mode, const char* command_string) {

		if (mode == Cmd_Execute_Mode::NOW)
			execute_string(command_string);
		else {
			command_buffer += command_string;
			command_buffer += '\n';
		}
	}
	void append_cmd(const std::string& msg) final {
		command_buffer += msg;
		command_buffer += '\n';
	}
	void append_cmd(uptr<SystemCommand> cmd) final {
		if (!cmd) {
			sys_print(Warning, "tried to append null cmd\n");
			return;
		}
		systemCommands.push_back(std::move(cmd));
	}

	void execute_cmd(const std::string& msg) final {
		execute_string(msg.c_str());
	}
	void execute_file(Cmd_Execute_Mode mode, const char* path) {

		auto file = FileSys::open_read_engine(path);
		if(!file) {
			sys_print(Error, "couldn't open config file to execute: %s\n", path);
			return;
		}
		
		DictParser parser;
		parser.load_from_file(file.get());

		StringView view;
		while (parser.read_line(view)) {
			if (view.is_empty())
				continue;

			view = strip_string_view(view);

			if (view.str_start[0] == '#' || (view.str_len==1&&view.str_start[0]=='\r'))
				continue;
			if (view.str_len >= 2 && view.str_start[0] == '/' && view.str_start[1] == '/')
				continue;

			std::string str = std::string(view.str_start, view.str_len);
			execute(Cmd_Execute_Mode::NOW, str.c_str());
		}
	}
	void execute_buffer() {
		auto execute_string_buffer = [&] {
			if (command_buffer.empty())
				return;
			std::string line;
			for (char c : command_buffer) {
				if (c == '\n') {
					if (!line.empty())
						execute_string(line.c_str());
					line.clear();
				}
				else {
					line += c;
				}
			}
			if (!line.empty())
				execute_string(line.c_str());
			command_buffer.clear();
		};
		auto execute_object_commands = [&]() {
			// commands can append to the systemCommands, it will still work and execute all of them
			for (int i = 0; i < systemCommands.size(); i++) {
				auto cmd = systemCommands[i].get();
				assert(cmd);
				sys_print(Info, "executing: %s\n", cmd->to_string().c_str());
				cmd->execute();
			}
			systemCommands.clear();	// calls destructors
		};
		execute_string_buffer();
		execute_object_commands();	// called after, string commands can add to object commands
	}

	void set_set_unknown_variables(bool b) override {
		set_unknown_variables = b;
	}



	Engine_Cmd_Function find_cmd(const std::string& str) {
		auto find = all_cmds.find(str);
		return find == all_cmds.end() ? nullptr : find->second;
	}
	std::function<void(const Cmd_Args&)>* find_cmd_in_groups(const std::string& str) {
		for (auto& c : cmd_groups) {
			if (!c->enabled)
				continue;
			auto find = c->cmds.find(str);
			if (find != c->cmds.end())
				return &find->second;
		}
		return nullptr;
	}
	
	std::string command_buffer;
	bool set_unknown_variables = false;
	std::unordered_map<std::string, Engine_Cmd_Function> all_cmds;
	std::vector<uptr<SystemCommand>> systemCommands;


	void add_cmd_group(ConsoleCmdGroup* group) {
		cmd_groups.insert(group);
	}
	void remove_cmd_group(ConsoleCmdGroup* group) {
		cmd_groups.erase(group);
	}
	std::unordered_set<ConsoleCmdGroup*> cmd_groups;
};

const char* Cmd_Args::at(int index) const {
	ASSERT(index >= 0 && index < argc&& index < MAX_ARGS);
	return &buffer[arg_to_index[index]];
}

void Cmd_Args::add_arg(const char* v, int len) {
	if (len + 1 + buffer_index >= BUFFER_SIZE || argc >= MAX_ARGS) return;
	memcpy(buffer + buffer_index, v, len);
	buffer[buffer_index + len] = 0;
	arg_to_index[argc] = buffer_index;
	buffer_index += len + 1;
	argc++;
}

uptr<ConsoleCmdGroup> ConsoleCmdGroup::create(std::string name)
{
	auto p = uptr< ConsoleCmdGroup>(new ConsoleCmdGroup);
	p->groupname = name;
	auto impl = (Cmd_Manager_Impl*)Cmd_Manager::get();
	impl->add_cmd_group(p.get());
	return std::move(p);
}
ConsoleCmdGroup::~ConsoleCmdGroup()
{
	auto impl = (Cmd_Manager_Impl*)Cmd_Manager::get();
	impl->remove_cmd_group(this);
}
ConsoleCmdGroup& ConsoleCmdGroup::add(std::string name, const std::function<void(const Cmd_Args&)>& func)
{
	cmds.insert({ name,func });
	return *this;
}
Cmd_Manager* Cmd_Manager::inst = nullptr;

Cmd_Manager* Cmd_Manager::create()
{
	return new Cmd_Manager_Impl;
}
