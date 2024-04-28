#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cassert>
#include "ScriptVars.h"
#include "StringUtil.h"

using std::string;
using std::vector;
using std::unordered_map;

class LispError
{
public:
	LispError(const char* msg, void* exp) : msg(msg) {}
	const char* msg;
};


class BytecodeStack
{
public:
	BytecodeStack(Parameter* stack, int sp, int size) : stack(stack), stack_ptr(sp), stack_size(size) {}

	Parameter pop() {
		assert(stack_ptr > 0);
		return stack[--stack_ptr];
	}
	void push(Parameter val) {
		assert(stack_ptr < stack_size);
		stack[stack_ptr++] = val;
	}

	Parameter* stack = nullptr;
	int stack_ptr = 0;
	int stack_size = 0;
};

typedef void(*bytecode_function)(BytecodeStack& stack);

struct define_bytecode_func
{
	define_bytecode_func(const char* name, const char* out_arg, const char* in_arg, bytecode_function ptr)
		: name(name), arg_out(out_arg), arg_in(in_arg), ptr(ptr){}

	const char* name = "";
	bytecode_function ptr = nullptr;
	const char* arg_in = "";
	const char* arg_out = "";
};

class GlobalEnumDefMgr;
class BytecodeContext
{
public:
	void push_function(define_bytecode_func func) {
		native_funcs.push_back(func);
		func_map[func.name] = native_funcs.size() - 1;
	}
	void push_global_enum_def(GlobalEnumDefMgr* mgr);

	int find_function_index(const std::string& name) {
		if (func_map.find(name) != func_map.end())
			return func_map[name];
		return -1;
	}
	define_bytecode_func* get_function(int idx) {
		return &native_funcs.at(idx);
	}

	Parameter* find_constant(const std::string& name) {
		if (constants.find(name) != constants.end())
			return &constants[name];
		return nullptr;
	}

	std::unordered_map<std::string, Parameter> constants;
	std::vector<define_bytecode_func> native_funcs;
	std::unordered_map<std::string, int> func_map;
};
class BytecodeExpression
{
public:
	vector<uint8_t> instructions;

	const char* compile_new(BytecodeContext* ctx, ScriptVars_CFG* vars, const std::string& code);
	Parameter execute(BytecodeContext* global, const ScriptVars_RT& vars) const;
private:
	const char* compile_from_tokens(BytecodeContext* ctx, ScriptVars_CFG* vars, std::vector<StringView>& tokens);
	void write_4bytes_at_location(uint32_t bytes, int loc);

	void push_inst(uint8_t c);
	void push_4bytes(unsigned int x);
	uint32_t read_4bytes(int offset) const;
	uintptr_t read_8bytes(int offset) const;
};