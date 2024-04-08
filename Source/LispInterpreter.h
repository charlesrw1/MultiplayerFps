#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cassert>
#include "ScriptVars.h"

using std::string;
using std::vector;
using std::unique_ptr;
using std::unordered_map;


// Any sufficiently complicated C or Fortran program contains an ad hoc, 
// informally-specified, bug-ridden, slow implementation of half of Common Lisp.

struct Exp;
struct At_Node;

struct LispExp;
class LispError
{
public:
	LispError(const char* msg, LispExp* exp) : msg(msg), what(exp) {}
	const char* msg;
	LispExp* what;
};
struct LispArgs
{
	LispArgs(LispExp* args_, int count) : args(args_), argc(count) {}
	int count() {
		return argc;
	}
	LispExp& at(int i);
	LispExp* args;
	int argc;
};

typedef LispExp(*lispprocfunc_t)(LispArgs args);

struct LispExp
{
	enum exptype
	{
		list_type,
		float_type,
		int_type,
		symbol_type,
		builtin_func_type,
		animation_tree_node_type,
		animation_transition_type,
		expression_type
	}type;
	LispExp() : type(int_type){}
	~LispExp() {
		if (type == symbol_type)
			delete u.sym;
		else if (type == list_type)
			delete u.list;
	}
	LispExp(exptype customtype, void* ptr) : type(customtype) {
		u.ptr = ptr;
	}
	LispExp(const LispExp& other) {
		switch (other.type) {
		case symbol_type:
			u.sym = new string(*other.u.sym);
			break;
		case list_type:
			u.list = new vector<LispExp>(*other.u.list);
			break;
		default:
			u = other.u;
			break;
		}
		type = other.type;
	}
	LispExp& operator=(const LispExp& other) {
		if (type != other.type) {
			if (type == symbol_type) delete u.sym;
			else if (type == list_type) delete u.list;
			switch (other.type) {
			case symbol_type:
				u.sym = new string(*other.u.sym);
				break;
			case list_type:
				u.list = new vector<LispExp>(*other.u.list);
				break;
			default:
				u = other.u;
				break;
			}
			type = other.type;
		}
		else {
			switch (type) {
			case symbol_type:
				*u.sym = *other.u.sym;
				break;
			case list_type:
				*u.list = *other.u.list;
				break;
			default:
				u = other.u;
				break;
			}
		}
		return *this;
	}
	LispExp(LispExp&& other) {
		switch (other.type) {
		case symbol_type:
			u.sym = other.u.sym;
			other.u.sym = nullptr;
			break;
		case list_type:
			u.list = other.u.list;
			other.u.list = nullptr;
			break;
		default:
			u = other.u;
			break;
		}
		type = other.type;
	}
	LispExp(vector<LispExp>&& values) {
		type = list_type;
		u.list = new vector<LispExp>(std::move(values));
	}
	LispExp(string&& sym) {
		type = symbol_type;
		u.sym = new string(std::move(sym));
	}

	int cast_to_int() {
		if (type == int_type) return u.i;
		else if (type == float_type)return u.f;
		else throw LispError("cant cast to int", this);
	}
	float cast_to_float() {
		if (type == int_type) return u.i;
		else if (type == float_type)return u.f;
		else throw LispError("cant cast to float", this);
	}

	void*& as_custom_type(exptype customtype) {
		if (type != customtype) throw LispError("expected custom type", this);
		return u.ptr;
	}
	int& as_int() {
		if (type != int_type) throw LispError("expected int", this);
		return u.i;
	}
	double& as_float() {
		if (type != float_type) throw LispError("expected float", this);
		return u.f;
	}
	string& as_sym() {
		if (type != symbol_type) throw LispError("expected symbol", this);
		return *u.sym;
	}
	vector<LispExp>& as_list() {
		if (type != list_type) throw LispError("expected list", this);
		return *u.list;
	}
	lispprocfunc_t as_builtin() {
		if (type != builtin_func_type) throw LispError("expected builtin", this);
		return u.proc;
	}

	union myunion {
		int i;
		double f;
		void* ptr;
		string* sym;
		vector<LispExp>* list;
		lispprocfunc_t proc;
	}u;
	explicit LispExp(int i) : type(int_type) {
		u.i = i;
	}
	explicit LispExp(double f) : type(float_type) {
		u.f = f;
	}
	LispExp(lispprocfunc_t proc) : type(builtin_func_type) {
		u.proc = proc;
	}

	void assign(LispExp other) {
		assert(!(type == list_type || type == symbol_type || other.type == list_type || other.type == symbol_type));
		type = other.type;
		u = other.u;
	}
};

inline LispExp& LispArgs::at(int i)
 {
	if (i >= argc) throw LispError("expected arg", nullptr);
	return args[i];
}

class Env
{
public:
	std::unordered_map<string, LispExp> symbols;
	const Exp* current_exp;
};


struct Interpreter_Ctx
{
	Interpreter_Ctx(int count, Env* e) : env(e){
		argbuf.reserve(count);
	}

	vector<LispExp> argbuf;
	int arg_head = 0;

	int allocate(int n) {
		return argbuf.size();
	}
	void free(int where) {
		argbuf.resize(where);
	}
	vector<LispExp>& get_buf() {
		return argbuf;
	}
	Env& get_env() {
		return *env;
	}

	Env* env;
};

class LispLikeInterpreter
{
public:
	static void execute(string code);
	static LispExp parse(string& code);

	static LispExp to_atom(string& token);

	static LispExp read_from_tokens(vector<string>& tokens);
	static LispExp eval(LispExp& exp, Interpreter_Ctx* ctx);
	static const Env& get_global_env();
};

class BytecodeExpression
{
public:
	vector<uint8_t> instructions;
	bool fast_path = false;
	handle<Parameter> param;

	Parameter execute(const ScriptVars_RT& vars) const;
	script_parameter_type compile(LispExp& exp, ScriptVars_CFG& vars);
private:
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
	uintptr_t read_8bytes(int offset) const {
		uintptr_t low = read_4bytes(offset);
		uintptr_t high = read_4bytes(offset + 4);
		return low | (high << 32);
	}
};