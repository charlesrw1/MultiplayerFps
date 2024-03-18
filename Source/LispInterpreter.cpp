#include "LispInterpreter.h"

static void replace(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); 
    }
}

static vector<string> to_tokens(string& input)
{
	vector<string> output;
	string token;
	for (char c : input) {
		if (c == ' ' || c == '\t' || c== '\n') {
			if (!token.empty()) {
				output.push_back(token);
				token.clear();
			}
		}
		else {
			token += c;
		}
	}
	if (!token.empty())
		output.push_back(token);
	return output;
}

	unique_ptr<Exp> LispLikeInterpreter::parse(string& code) {
		replace(code, "(", " ( ");
		replace(code, ")", " ) ");
		auto tokens = to_tokens(code);
		std::reverse(tokens.begin(), tokens.end());
		try {
			return read_from_tokens(tokens);
		}
		catch (const char* err) {
			printf("parse error: %s\n", err);
			return nullptr;
		}
	}

	Atom LispLikeInterpreter::to_atom(string& token) {
		if (token.empty())
			throw "empty atom";
			
		if (isdigit(token.at(0))) {
			if(token.find('.')!=string::npos)
				return Atom(atof(token.c_str()));
			else 
				return Atom(atoi(token.c_str()));
		}
		else
			return Atom(token);
	}

	unique_ptr<Exp> LispLikeInterpreter::read_from_tokens(vector<string>& tokens) {
		if (tokens.empty()) {
			throw "unexpected eof";
		}
		string token = tokens.back();
		tokens.pop_back();
		if (token == "(") {
			unique_ptr<Exp> exp(new Exp);
			exp->type = Exp::list_type;
			while (tokens.back() != ")")
				exp->list.push_back(read_from_tokens(tokens));
			tokens.pop_back();
			return std::move(exp);
		}
		else if (token == ")") {
			throw "unexpected ')'";
		}
		else {
			Exp* exp = new Exp;
			exp->type = Exp::atom_type;
			exp->atom = to_atom(token);
			return unique_ptr<Exp>(exp);
		}
	}

	Atom_Or_Proc LispLikeInterpreter::eval(Exp* exp, Interpreter_Ctx* ctx)
	{
		auto exptype = exp->type;
		// atom types
		if (exptype == Exp::atom_type) {
			auto atomtype = exp->atom.type;
			if (atomtype == Atom::symbol_type) {
				const auto& find = ctx->env->symbols.find(exp->atom.sym);
				if (find != ctx->env->symbols.end())
					return find->second;
				else
					return exp->atom;
			}
			else
				return exp->atom;
		}
		auto& op = exp->list.at(0);
		if (op->type != Exp::atom_type || op->atom.type != Atom::symbol_type)
			throw "needs symbol for start of list";
		
		if (op->atom.sym == "quote") {
			return Atom(exp->list.at(1).release());
		}

		Atom_Or_Proc proc = eval(op.get(), ctx);
		if (proc.type != Atom_Or_Proc::proc_type)
			return proc;

		int argc = exp->list.size() - 1;
		Atom* args = ctx->allocate(argc);
		for (int i = 1; i < exp->list.size(); i++) {
			Atom_Or_Proc proc = eval(exp->list.at(i).get(), ctx);
			if (proc.type != Atom_Or_Proc::atom_type)
				throw "cant have proc args";
			args[i - 1] = proc.a;
		}
		Atom ret = proc.proc(args,argc);
		for (int i = 0; i < argc; i++) {
			if (args[i].type == Atom::exp_type)
				delete args[i].exp;
		}
		ctx->arg_head -= argc;
		return ret;
	}

	int Atom::cast_to_int()
	{
		if (type == int_type)
			return i;
		else if (type == float_type)
			return int(f);
		else
			throw "cant cast to int";
	}
	double Atom::cast_to_float()
	{
		if (type == int_type)
			return double(i);
		else if (type == float_type)
			return f;
		else
			throw "cant cast to float";
	}

	Atom add_f(Atom* args, int argc)
	{
		if (argc != 2)
			throw "add: needs 2 arguments";
		auto type1 = args[0].type;
		auto type2 = args[1].type;

		if (type1 == Atom::int_type && type2 == Atom::int_type)
			return Atom(int(args[0].i + args[1].i));
		else
			return Atom(double(args[0].cast_to_float() + args[1].cast_to_float()));
	}
	Atom mult_f(Atom* args, int argc)
	{
		if (argc != 2)
			throw "mult: needs 2 arguments";
		auto type1 = args[0].type;
		auto type2 = args[1].type;
		if (type1 == Atom::int_type && type2 == Atom::int_type)
			return Atom(int(args[0].i * args[1].i));
		else
			return Atom(double(args[0].cast_to_float() * args[1].cast_to_float()));
	}
	Atom div_f(Atom* args, int argc)
	{
		if (argc != 2)
			throw "div: needs 2 arguments";
		auto type1 = args[0].type;
		auto type2 = args[1].type;

		if (type1 == Atom::int_type && type2 == Atom::int_type)
			return Atom(int(args[0].i / args[1].i));
		else
			return Atom(double(args[0].cast_to_float() / args[1].cast_to_float()));
	}
	Atom sub_f(Atom* args, int argc)
	{
		if (argc != 2)
			throw "sub: needs 2 arguments";
		auto type1 = args[0].type;
		auto type2 = args[1].type;

		if (type1 == Atom::int_type && type2 == Atom::int_type)
			return Atom(int(args[0].i - args[1].i));
		else
			return Atom(double(args[0].cast_to_float() - args[1].cast_to_float()));
	}
	Atom not_f(Atom* args, int argc)
	{
		if (argc != 1)
			throw "not: needs 1 arguments";
		return Atom(!args[0].cast_to_int());
	}
	Atom less_f(Atom* args, int argc)
	{
		if (argc != 2)
			throw "<: needs 2 arguments";
		auto type1 = args[0].type;
		auto type2 = args[1].type;
		if (type1 == Atom::int_type && type2 == Atom::int_type)
			return Atom(int(args[0].i < args[1].i));
		else
			return Atom(double(args[0].cast_to_float() < args[1].cast_to_float()));
	}
	Atom greater_f(Atom* args, int argc)
	{
		if (argc != 2)
			throw ">: needs 2 arguments";
		auto type1 = args[0].type;
		auto type2 = args[1].type;
		if (type1 == Atom::int_type && type2 == Atom::int_type)
			return Atom(int(args[0].i > args[1].i));
		else
			return Atom(double(args[0].cast_to_float() > args[1].cast_to_float()));
	}
	Atom greatereq_f(Atom* args, int argc)
	{
		if (argc != 2)
			throw ">=: needs 2 arguments";
		auto type1 = args[0].type;
		auto type2 = args[1].type;
		if (type1 == Atom::int_type && type2 == Atom::int_type)
			return Atom(int(args[0].i >= args[1].i));
		else
			return Atom(double(args[0].cast_to_float() >= args[1].cast_to_float()));
	}
	Atom lesseq_f(Atom* args, int argc)
	{
		if (argc != 2)
			throw "<=: needs 2 arguments";
		auto type1 = args[0].type;
		auto type2 = args[1].type;
		if (type1 == Atom::int_type && type2 == Atom::int_type)
			return Atom(int(args[0].i <= args[1].i));
		else
			return Atom(double(args[0].cast_to_float() <= args[1].cast_to_float()));
	}
	Atom eq_f(Atom* args, int argc)
	{
		if (argc != 2)
			throw "==: needs 2 arguments";
		auto type1 = args[0].type;
		auto type2 = args[1].type;
		if (type1 == Atom::int_type && type2 == Atom::int_type)
			return Atom(int(args[0].i == args[1].i));
		else
			return Atom(double(args[0].cast_to_float() == args[1].cast_to_float()));
	}
	Atom noteq_f(Atom* args, int argc)
	{
		if (argc != 2)
			throw "!=: needs 2 arguments";
		auto type1 = args[0].type;
		auto type2 = args[1].type;
		if (type1 == Atom::int_type && type2 == Atom::int_type)
			return Atom(int(args[0].i != args[1].i));
		else
			return Atom(double(args[0].cast_to_float() != args[1].cast_to_float()));
	}
	Atom and_f(Atom* args, int argc)
	{
		if (argc != 2)
			throw "and: needs 2 arguments";
		return Atom(args[0].cast_to_int() && args[1].cast_to_int());
	}
	Atom or_f(Atom* args, int argc)
	{
		if (argc != 2)
			throw "or: needs 2 arguments";
		return Atom(args[0].cast_to_int() || args[1].cast_to_int());
	}

	const Env& LispLikeInterpreter::get_global_env()
	{
		static bool initialized = false;
		static Env global;
		if (!initialized) {
			global.symbols["+"] = add_f;
			global.symbols["-"] = sub_f;
			global.symbols["/"] = div_f;
			global.symbols["*"] = mult_f;
			global.symbols["<"] = less_f;
			global.symbols[">"] = greater_f;
			global.symbols[">="] = greatereq_f;
			global.symbols["<="] = lesseq_f;
			global.symbols["!="] = noteq_f;
			global.symbols["=="] = eq_f;
			global.symbols["not"] = not_f;
			global.symbols["and"] = and_f;
			global.symbols["or"] = or_f;
		}
		return global;
	}