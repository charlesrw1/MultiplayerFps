#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cassert>
using std::string;
using std::vector;
using std::unique_ptr;
using std::unordered_map;


struct Exp;
struct At_Node;
struct Atom
{
	Atom() : type(int_type), i(0) {}
	explicit Atom(int in) : type(int_type), i(in) {}
	explicit Atom(double fl) : type(float_type), f(fl) {}
	Atom(string sym_) 
		: type(symbol_type)  {
		this->sym = std::move(sym_);
	}
	Atom(Exp* exp) : type(exp_type), exp(exp) {}
	Atom(At_Node* node) : ptr(node), type (animation_tree_node_type) {}
	

	enum type {
		symbol_type,
		float_type,
		int_type,
		animation_tree_node_type,
		animation_transition_type,
		exp_type,
	}type;
	union {
		int i;
		double f;
		void* ptr;
		Exp* exp;
	};
	string sym;

	int cast_to_int();
	double cast_to_float();
};

typedef Atom(*procfunc_t)(Atom* args, int argc);
struct Atom_Or_Proc
{
	Atom_Or_Proc(Atom a) : type(atom_type), a(a) {}
	Atom_Or_Proc(procfunc_t f) : type(proc_type), proc(f) {}
	Atom_Or_Proc() : type(atom_type), a(Atom(0)){}

	enum type {
		atom_type,
		proc_type,
	}type;
	Atom a;
	procfunc_t proc;
};


struct Exp
{
	Exp() {}
	Exp(Atom atom) : atom(atom), type(atom_type) {}
	enum type {
		atom_type,
		list_type
	}type;
	vector<unique_ptr<Exp>>list;
	Atom atom;
};

class Env
{
public:
	std::unordered_map<string, Atom_Or_Proc> symbols;
	const Exp* current_exp;
};

struct Interpreter_Ctx
{
	vector<Atom> argbuf;
	int arg_head = 0;
	Atom* allocate(int n) {
		int start = arg_head;
		arg_head += n;
		return &argbuf.at(start);
	}
	const Env* env;
};

class LispLikeInterpreter
{
public:
	static void execute(string code);
	static unique_ptr<Exp> parse(string& code);

	static Atom to_atom(string& token);

	static unique_ptr<Exp> read_from_tokens(vector<string>& tokens);
	static Atom_Or_Proc eval(Exp* exp, Interpreter_Ctx* ctx);
	static const Env& get_global_env();
};