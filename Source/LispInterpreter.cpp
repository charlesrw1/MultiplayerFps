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

vector<string> to_tokens(string& input)
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

	LispExp LispLikeInterpreter::parse(string& code) {
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

	LispExp LispLikeInterpreter::to_atom(string& token) {
		if (token.empty())
			throw "empty atom";
			
		if (isdigit(token.at(0))) {
			if(token.find('.')!=string::npos)
				return LispExp(atof(token.c_str()));
			else 
				return LispExp(atoi(token.c_str()));
		}
		else
			return LispExp(std::move(token));
	}

	LispExp LispLikeInterpreter::read_from_tokens(vector<string>& tokens) {
		if (tokens.empty()) {
			throw "unexpected eof";
		}
		string token = tokens.back();
		tokens.pop_back();
		if (token == "(") {
			vector<LispExp> list;
			while (tokens.back() != ")")
				list.push_back(read_from_tokens(tokens));
			tokens.pop_back();
			return LispExp(std::move(list));
		}
		else if (token == ")") {
			throw "unexpected ')'";
		}
		else {
			return to_atom(token);
		}
	}

	LispExp LispLikeInterpreter::eval(LispExp& exp, Interpreter_Ctx* ctx)
	{
		auto exptype = exp.type;
		// atom types
		if (exptype == LispExp::symbol_type) {
			const auto& find = ctx->env->symbols.find(exp.as_sym());
			if (find != ctx->env->symbols.end())
				return LispExp(find->second);
			return LispExp(std::move(exp));
		}
		else if (exptype != LispExp::list_type) {
			return LispExp(std::move(exp));
		}
		// list type
		auto& op = exp.as_list().at(0);
		if (op.as_sym() == "quote")
			return LispExp(exp.as_list().at(1));

		LispExp proc = eval(op, ctx);

		int argc = exp.as_list().size() - 1;
		int head = ctx->allocate(argc);
		vector<LispExp>& exps = ctx->argbuf;
		for (int i = 0; i < argc; i++) {
			exps.push_back( std::move(eval(exp.as_list().at(i+1), ctx)) );
		}
		LispArgs args = LispArgs(&exps.at(head), argc);
		LispExp returnval = proc.as_builtin()(args);
		ctx->free(head);
	
		return returnval;
	}

#define BASE_OP(name, op) \
	LispExp name(LispArgs args) \
	{	\
		auto type1 = args.at(0).type;	\
		auto type2 = args.at(1).type;	\
		if (type1 == LispExp::int_type && type2 == LispExp::int_type)	\
			return LispExp(args.at(0).u.i op args.at(1).u.i);	\
		else \
			return LispExp(double(args.at(0).cast_to_float() op args.at(1).cast_to_float())); \
	}
BASE_OP(add_f, +)
BASE_OP(sub_f, -)
BASE_OP(div_f,/)
BASE_OP(mult_f,*)
BASE_OP(lt_f, <)
BASE_OP(leq_f,<=)
BASE_OP(gt_f,>)
BASE_OP(geq_f,>=)
BASE_OP(eq_f,==)
BASE_OP(noteq_f,!=)

	LispExp not_f(LispArgs args)
	{
		return LispExp(args.at(0).cast_to_int());
	}

	LispExp and_f(LispArgs args)
	{
		
		return LispExp(args.at(0).cast_to_int() && args.at(1).cast_to_int());
	}
	LispExp or_f(LispArgs args)
	{
		return LispExp(args.at(0).cast_to_int() || args.at(1).cast_to_int());
	}

	LispExp list_f(LispArgs args)
	{
		vector<LispExp> list;
		for (int i = 0; i < args.count(); i++)
			list.push_back( std::move(args.args[i]) );
		return LispExp(std::move(list));
	}
	LispExp islist_f(LispArgs args)
	{
		return LispExp( args.at(0).type == LispExp::list_type );
	}

	const Env& LispLikeInterpreter::get_global_env()
	{
		static bool initialized = false;
		static Env global;

		if (!initialized) {
			global.symbols.emplace("+", add_f);
			global.symbols.emplace("-", sub_f);
			global.symbols.emplace("/",div_f);
			global.symbols.emplace("*", mult_f);
			global.symbols.emplace("<", lt_f);
			global.symbols.emplace(">", gt_f);
			global.symbols.emplace(">=",geq_f);
			global.symbols.emplace("<=",leq_f);
			global.symbols.emplace("!=",noteq_f);
			global.symbols.emplace("==", eq_f);
			global.symbols.emplace("not", not_f);
			global.symbols.emplace("and", and_f);
			global.symbols.emplace("or", or_f);
			global.symbols.emplace("list", list_f);
			global.symbols.emplace("list?", islist_f);
		}
		return global;
	}

	
enum opcode : uint16_t
{
	ADD_F,
	ADD_I,
	SUB_F,
	SUB_I,
	MULT_F,
	MULT_I,
	DIV_F,
	DIV_I,

	LT_F,
	LT_I,
	LEQ_F,
	LEQ_I,
	GT_F,
	GT_I,
	GEQ_F,
	GEQ_I,

	
	EQ_F,
	EQ_I,
	NOTEQ_F,
	NOTEQ_I,

	AND,
	OR,
	NOT,
	PUSH_CONST_F,
	PUSH_CONST_I,
	PUSH_F,
	PUSH_I,
	CAST_0_F,
	CAST_0_I,
	CAST_1_F,
	CAST_1_I,
};



	LispBytecode::type LispBytecode::compile(LispExp& exp, Env* env) {
		if (exp.type != LispExp::list_type) {
			if (exp.type == LispExp::symbol_type) {
				const auto& find = env->symbols.find(exp.as_sym());
				if (find != env->symbols.end()) {
					auto& findexp = find->second;
					if (findexp.type == LispExp::int_type) {
						push_inst(PUSH_I);
						uintptr_t ptr = (uintptr_t)&findexp.u.i;
						push_4bytes(ptr);
						push_4bytes(ptr >> 32);
						return type::int_type;
					}
					else if (find->second.type == LispExp::float_type) {
						push_inst(PUSH_F);
						uintptr_t ptr = (uintptr_t)&findexp.u.f;
						push_4bytes(ptr);
						push_4bytes(ptr >> 32);
						return type::float_type;
					}
					else
						throw LispError("can only compile int/floats", &exp);
				}
				else
					throw LispError("undefined symbol", &exp);
			}
			else if (exp.type == LispExp::int_type) {
				push_inst(PUSH_CONST_I);
				push_4bytes(exp.u.i);
				return type::int_type;
			}
			else if (exp.type == LispExp::float_type) {
				push_inst(PUSH_CONST_F);
				float f = exp.u.f;
				push_4bytes(*(unsigned int*)&f);
				return type::float_type;
			}
			else {
				throw LispError("unknown type for compiling", &exp);
			}
		}
		else {
			string op = exp.as_list().at(0).as_sym();
			if (op == "not") {
				auto type1 = compile( exp.as_list().at(1) , env);
				if (type1 == type::float_type) {
					push_inst(CAST_0_I);
				}
				push_inst(NOT);
				return type::int_type;
			}
			else if (op == "and" || op == "or") {
				auto type1 = compile(exp.as_list().at(1), env);
				auto type2 = compile(exp.as_list().at(2), env);
				if (type1 != int_type)
					push_inst(CAST_1_I);
				if (type2 != int_type)
					push_inst(CAST_0_I);
				if (op == "and")
					push_inst(AND);
				else
					push_inst(OR);
				return type::int_type;
			}
			else {
				auto type1 = compile(exp.as_list().at(1), env);
				auto type2 = compile(exp.as_list().at(2), env);
				bool isfloat = (type1 == float_type || type2 == float_type);
				if (isfloat && type1 != float_type)
					push_inst(CAST_1_F);
				if (isfloat && type2 != float_type)
					push_inst(CAST_0_F);
				struct pairs {
					const char* name;
					opcode op;
				};
				const static pairs p[] = {
					{"+",ADD_F},
					{"-",SUB_F},
					{"*",MULT_F},
					{"/",DIV_F},
					{"<",LT_F},
					{">",GT_F},
					{"<=",LEQ_F},
					{">=",GEQ_F},
					{"==",EQ_F},
					{"!=",NOTEQ_F}
				};
				int i = 0;
				for (; i < 10; i++) {
					if (p[i].name == op) {
						if (isfloat) push_inst(p[i].op);
						else push_inst(p[i].op + 1);
						break;
					}
				}
				if (i == 10) throw LispError("couldn't find op", nullptr);

				return (isfloat) ? float_type : int_type;
			}
		}
	}

#define OPONSTACK(op, typein, typeout) stack[sp-2].typeout = stack[sp-2].typein op stack[sp-1].typein; sp-=1; break;
#define FLOAT_AND_INT_OP(opcode_, op) case opcode_: OPONSTACK(op, f, f); \
case (opcode_+1): OPONSTACK(op,i,i);
#define FLOAT_AND_INT_OP_OUTPUT_INT(opcode_, op) case opcode_: OPONSTACK(op,f, i); \
case (opcode_+1): OPONSTACK(op,i, i);
	LispBytecode::stack_val LispBytecode::execute()
	{
		stack_val stack[64];
		int sp = 0;

		for (int pc = 0; pc < instructions.size(); pc++) {
			uint8_t op = instructions[pc];
			switch (op)
			{
				FLOAT_AND_INT_OP(ADD_F, +);
				FLOAT_AND_INT_OP(SUB_F, -);
				FLOAT_AND_INT_OP(MULT_F, *);
				FLOAT_AND_INT_OP(DIV_F, / );
				FLOAT_AND_INT_OP_OUTPUT_INT(LT_F, < );
				FLOAT_AND_INT_OP_OUTPUT_INT(LEQ_F, <= );
				FLOAT_AND_INT_OP_OUTPUT_INT(GT_F, > );
				FLOAT_AND_INT_OP_OUTPUT_INT(GEQ_F, >= );
				FLOAT_AND_INT_OP_OUTPUT_INT(EQ_F, == );
				FLOAT_AND_INT_OP_OUTPUT_INT(NOTEQ_F, != );

			case NOT:
				stack[sp-1].i = !stack[sp-1].i;
				break;
			case AND:
				stack[sp - 2].i = stack[sp - 2].i && stack[sp - 1].i;
				sp -= 1;
				break;
			case OR:
				stack[sp - 2].i = stack[sp - 2].i || stack[sp - 1].i;
				sp -= 1;
				break;

			case CAST_0_F:
				stack[sp-1].f = stack[sp-1].i;
				break;
			case CAST_0_I:
				stack[sp-1].i = stack[sp-1].f;
				break;
			case CAST_1_F:
				stack[sp - 2].f = stack[sp - 2].i;
				break;
			case CAST_1_I:
				stack[sp - 2].i = stack[sp - 2].f;
				break;

			case PUSH_CONST_F: {
				union {
					int i;
					float f;
				};
				i = read_4bytes(pc + 1);
				stack[sp++].f = f;
				pc += 4;
			} break;
			case PUSH_CONST_I: {
				int i = read_4bytes(pc + 1);
				stack[sp++].i = i;
				pc += 4;
			}break;
			case PUSH_F: {
				unsigned long long ptr = read_8bytes(pc + 1);
				stack[sp++].f = *((double*)ptr);
				pc += 8;
			}break;
			case PUSH_I: {
				unsigned long long ptr = read_8bytes(pc + 1);
				stack[sp++].i = *((int*)ptr);
				pc += 8;
			}break;
			}
		}
		assert(sp == 1);
		return stack[0];
	}

#undef FLOAT_AND_INT_OP
#undef FLOAT_AND_INT_OP_OUTPUT_INT
#undef OPONSTACK
