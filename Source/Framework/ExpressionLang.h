#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cassert>

#include "ScriptValue.h"

#include "Framework/StringName.h"
#include "Framework/StringUtil.h"
#include "Framework/InlineVec.h"
#include "Framework/Handle.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ClassBase.h"

class CompileError
{
public:
	CompileError(std::string msg, int line) : str(msg), line(line) {}
	void print() {
		printf("[line %d] %s\n", line, str.c_str());
	}
	std::string str;
	int line = -1;
};

struct script_state;
typedef void(*bytecode_function)(script_state* stack);



enum class script_types : uint8_t
{
	bool_t,		// 'b' 4 byte true/false
	int_t,		// 'i' 4 byte integer
	float_t,	// 'f' 4 byte float
	name_t,		// 'n' 8 byte hashed string
	pointer_t,	// 'p' 8 byte custom pointer with type name 'mypointer_t', 'p' for untyped void* (dont do this)
	custom_t,	// 's' custom type, arb size, only can be used by user functions
	empty_t,	// 'e' 0 byte return
};
ENUM_HEADER(script_types);


enum class script_def_type : uint8_t
{
	nativefunc_t,		// native function pointer
	constant_t,			// constant variable/enum
	struct_t,			// structure def
};

struct script_variable_def
{
	std::string name;
	script_def_type type;
	union {
		struct nativefunc_def {
			bytecode_function ptr;
			const char* argin;
			const char* argout;
			int index;
		}func;
		struct constvar_def {
			script_types type;
			script_value_t value;
		}constant;
		struct struct_def {
			const char* fields;
			int index;
		}struct_;
	};
};


class script_state
{
public:
	script_state(script_value_t* stack, int sp, int size, void* user = nullptr)
		: stack(stack), stack_ptr(sp), stack_size(size), user_ptr(user) {}

	void* get_user_ptr() const { return user_ptr; }

	StringName pop_name() {
		assert(stack_ptr > 0);
		name_hash_t n = stack[--stack_ptr].ui64;
		return StringName(n);
	}
	int pop_int() {
		assert(stack_ptr > 0);
		float i = stack[--stack_ptr].ui32;
		return i;
	}
	float pop_float() {
		assert(stack_ptr > 0);
		float f = stack[--stack_ptr].f;
		return f;
	}

	void push_int(int i) {
		assert(stack_ptr < stack_size);
		stack[stack_ptr++].ui32 = i;
	}
	void push_float(float f) {
		assert(stack_ptr < stack_size);
		stack[stack_ptr++].f = f;
	}
	void push_name(StringName name) {
		assert(stack_ptr < stack_size);
		stack[stack_ptr++].ui64 = name.get_hash();
	}

	void* user_ptr = nullptr;
	script_value_t* stack = nullptr;
	int stack_ptr = 0;
	int stack_size = 0;

};

struct compile_result
{
	void push_std_type(script_types t) {
		out_types.push_back(std::move(t));
	}
	void push_custom_type(uint16_t index) {
		out_types.push_back(script_types::custom_t);
		custom_types.push_back(std::move(index));
	}
	void clear() { out_types.resize(0); custom_types.resize(0); }

	InlineVec<script_types, 16> out_types;
	InlineVec<uint16_t, 2> custom_types;
};

struct StringViewAndLine
{
	StringViewAndLine(const char* ptr, int count, int line_num) : view(StringView(ptr, count)), line_num(line_num) {}
	StringView view;
	int line_num = -1;
};

struct compile_local_variable
{
	StringName name;
	int offset = 0;
	StringView view;
	compile_result type;
};

struct compiled_function
{
	compile_result args;
	compile_result out;
	int program_location = -1;
	StringView view;
	StringName name;
};

struct fixup_func
{
	int byte_location = -1;
	int compilied_func_index = -1;
};

struct compile_data_referenced
{
	const compile_local_variable* find_local(StringName name) const {
		for (int i = 0; i < vars.size(); i++)
			if (vars[i].name == name) return vars.data() + i;
		return nullptr;
	}

	std::vector<compile_local_variable> vars;
	//std::vector<compiled_function> funcs;
	//std::vector<fixup_func> fixups;
};



struct PropertyInfoList;
struct PropertyInfo;

struct ScriptVariable
{
	std::string name;
	script_types type = script_types::bool_t;
	bool is_native = false;
	int var_offset = 0;
	
	static const PropertyInfoList* get_props();

	// set at runtime initialization
	const PropertyInfo* native_pi_of_variable = nullptr;
};

class Script;
typedef handle<Script> ScriptHandle;
class ScriptInstance;
class Script
{
public:
	static const PropertyInfoList* get_props();

	Script() = default;
	Script(std::vector<ScriptVariable>& variables, std::string native_classname);

	bool execute(ScriptHandle function_handle, script_state* state, ScriptInstance* input) const;
	compile_result compile(ScriptHandle& handle, const std::string& code, const std::string& selftype);
	void print_instructions(ScriptHandle handle) const;

	void link_to_native_class();
	bool check_is_valid();
	const ClassTypeInfo* get_native_class() const {return native_classtypeinfo; }

	uint32_t num_variables() const { return variables.size(); }
	uint32_t get_num_values_for_variables() const { return num_values_for_variables; }

	const std::vector<ScriptVariable>& get_variables() const { return variables; };
private:
	uint32_t read_4bytes(uint32_t offset) const {
		uint32_t res = (instruction_data[offset]);
		res |= ((uint32_t)instruction_data[offset + 1] << 8);
		res |= ((uint32_t)instruction_data[offset + 2] << 16);
		res |= ((uint32_t)instruction_data[offset + 3] << 24);
		return res;
	}
	float read_variable_float(uint32_t variable, ScriptInstance* inst) const;
	int32_t read_variable_int32(uint32_t variable, ScriptInstance* inst) const;
	const ScriptVariable* find_variable(const std::string& name) const {
		for (int i = 0; i < variables.size(); i++)
			if (variables[i].name == name)
				return &variables[i];
		return nullptr;
	}
	uint32_t variable_index(const ScriptVariable* v) const { return v - variables.data(); }

	struct ScriptFunction {
		uint32_t bytecode_offset = 0;
		uint32_t bytecode_length = 0;
		uint8_t input_vars = 0;
		uint8_t output_vars = 0;
	};
	std::vector<uint8_t> instruction_data;
	std::vector<char> string_data;
	std::vector<ScriptFunction> function_ptrs;

	std::vector<ScriptVariable> variables;
	uint32_t num_values_for_variables = 0;
	std::string native_classname;
	const ClassTypeInfo* native_classtypeinfo = nullptr;

	friend class BytecodeCompileHelper;
};
class ScriptInstance
{
public:
	bool init_from(const Script* script, ClassBase* base_class);
	ClassBase* get_native_class_ptr() const { return native_class_ptr;  }
	const Script* get_parent_script() const { return instance_of; }
	std::vector<script_value_t>& get_values_array() { return values; }
private:
	// variable data
	const Script* instance_of = nullptr;
	ClassBase* native_class_ptr = nullptr;
	std::vector<script_value_t> values;
};

class Library
{
public:
	std::unordered_map<std::string, int> name_to_idx;
	std::vector<script_variable_def> defs;
	uint16_t num_funcs = 0;
	uint16_t num_structs = 0;

	void push_function_def(const char* name, const char* out /* comma seperated out type(s)*/, const char* in, bytecode_function func);
	void push_constant_def(const char* name, script_types type, script_value_t value);
	void push_struct_def(const char* name, const char* fields);
	const script_variable_def* find_def(const std::string& s) const;
};

class Program
{
public:
	static Program& get() {
		static Program inst;
		return inst;
	}
	void add_library(const Library* lib);
private:
	struct full_import {
		const Library* lib = nullptr;
		uint16_t func_start = 0;
		uint16_t struct_start = 0;
	};

	InlineVec<full_import, 2> imports;

	struct runtime_func_ptr {
		bytecode_function function;
#ifdef DEBUG_SCRIPT
		const Library* lib = nullptr;
		const script_variable_def* def = nullptr;
#endif
	};
	std::vector<runtime_func_ptr> func_ptrs;


	struct find_tuple {
		const script_variable_def* def = nullptr;
		int full_index = 0;	// for variables and functions
	};


	find_tuple find_def(const std::string& name) const;


	friend class Script;
	friend class BytecodeCompileHelper;
private:
	Program() {}
};