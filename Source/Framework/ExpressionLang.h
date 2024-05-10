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


enum class script_def_type : uint8_t
{
	nativefunc_t,		// native function pointer
	global_t,			// global input variable, provide index
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
		struct globalvar_def {
			script_types type;
			uint64_t name_hash;
			int index;
		}global;
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

class Program;
class BytecodeExpression
{
public:
	std::vector<uint8_t> instructions;

	compile_result compile(const Program* expr, const std::string& code, StringName self_type);
	void execute(script_state* state, const Program* prog, program_script_vars_instance* input) const;
	void print_instructions();
private:
	void compile_from_tokens(compile_result& result, compile_data_referenced& local, int current_base_ptr, const Program* ctx, std::vector<StringViewAndLine>& tokens, StringName self_type);

	void write_4bytes_at_location(uint32_t bytes, int loc) {
		instructions.at(loc) = (bytes & 0xff);
		instructions.at(loc + 1) = (bytes >> 8) & 0xff;
		instructions.at(loc + 2) = (bytes >> 16) & 0xff;
		instructions.at(loc + 3) = (bytes >> 24) & 0xff;
	}

	void push_inst(uint8_t c) {
		instructions.push_back(c);
	}
	void push_4bytes(unsigned int x) {
		instructions.push_back(x & 0xff);
		instructions.push_back((x >> 8) & 0xff);
		instructions.push_back((x >> 16) & 0xff);
		instructions.push_back((x >> 24) & 0xff);
	}
	uint32_t read_4bytes(int offset) const {
		uint32_t res = (instructions[offset]);
		res |= ((uint32_t)instructions[offset + 1] << 8);
		res |= ((uint32_t)instructions[offset + 2] << 16);
		res |= ((uint32_t)instructions[offset + 3] << 24);
		return res;
	}

	friend class Program;
};


class Library
{
public:
	std::unordered_map<uint64_t, int> name_to_idx;
	std::vector<script_variable_def> defs;
	uint16_t num_vars = 0;
	uint16_t num_funcs = 0;
	uint16_t num_structs = 0;

	void push_global_def(const char* name, script_types type);
	void push_function_def(const char* name, const char* out /* comma seperated out type(s)*/, const char* in, bytecode_function func);
	void push_constant_def(const char* name, script_types type, script_value_t value);
	void push_struct_def(const char* name, const char* fields);
	const script_variable_def* find_def(StringName name) const;
	void clear() {
		num_vars = num_funcs = num_structs = 0;
		defs.clear();
		name_to_idx.clear();
	}
};



class Program
{
public:
	void clear() {
		imports.resize(0);
		func_ptrs.resize(0);
	}

	struct full_import {
		const Library* lib = nullptr;
		uint16_t func_start = 0;
		uint16_t var_start = 0;
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

	void push_library(const Library* lib);

	struct find_tuple {
		const script_variable_def* def = nullptr;
		int full_index = 0;	// for variables and functions
	};

	find_tuple find_def(const char* name) const {
		return find_def(StringName(name));
	}
	find_tuple find_def(StringName name) const;
	int find_variable_index(StringName name) const {
		return find_def(name).full_index;
	}
	int num_vars() const {
		return imports.size() > 0 ?
			imports[imports.size() - 1].var_start + imports[imports.size() - 1].lib->num_vars
			: 0;
	}

	void print_globals() const;
	void print_functions() const;
};