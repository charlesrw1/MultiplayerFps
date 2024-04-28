#include "ExpressionLang.h"


inline void BytecodeExpression::write_4bytes_at_location(uint32_t bytes, int loc) {
	instructions.at(loc) = (bytes & 0xff);
	instructions.at(loc + 1) = (bytes >> 8) & 0xff;
	instructions.at(loc + 2) = (bytes >> 16) & 0xff;
	instructions.at(loc + 3) = (bytes >> 24) & 0xff;
}

inline void BytecodeExpression::push_inst(uint8_t c) {
	instructions.push_back(c);
}

inline void BytecodeExpression::push_4bytes(unsigned int x) {
	instructions.push_back(x & 0xff);
	instructions.push_back((x >> 8) & 0xff);
	instructions.push_back((x >> 16) & 0xff);
	instructions.push_back((x >> 24) & 0xff);
}

inline uint32_t BytecodeExpression::read_4bytes(int offset) const {
	uint32_t res = (instructions[offset]);
	res |= ((uint32_t)instructions[offset + 1] << 8);
	res |= ((uint32_t)instructions[offset + 2] << 16);
	res |= ((uint32_t)instructions[offset + 3] << 24);
	return res;
}

inline uintptr_t BytecodeExpression::read_8bytes(int offset) const {
	uintptr_t low = read_4bytes(offset);
	uintptr_t high = read_4bytes(offset + 4);
	return low | (high << 32);
}


static void replace(std::string& str, const std::string& from, const std::string& to) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}
static std::string to_upper(const std::string& s)
{
	std::string out = s;
	for (int i = 0; i < out.size(); i++)
		out[i] = toupper(out[i]);
}

std::vector<StringView> to_tokens(const string& input)
{
	std::vector<StringView> out;

	int start = 0;
	int count = 0;

	for (int idx = 0; idx < input.size(); idx++) {
		char c = input[idx];

		if (c == ' ' || c == '\t' || c == '\n') {
			if (count != 0) {
				out.push_back(StringView(input.data() + start, count));
			}
			count = 0;
		}
		else {
			if (count == 0)
				start = idx;
			count++;
		}
	}
	if (count != 0)
		out.push_back(StringView(input.data() + start, count));
	return out;
}
bool is_number(StringView token, bool* is_float)
{
	int start = 0;
	if (token.str_start[0] == '-')
		start = 1;
	bool back_had_float = false;
	int end = token.str_len;
	if (token.str_start[end - 1] == 'f') {
		back_had_float = true;
		end -= 1;
	}

	bool any_digit = false;
	bool seen_decimal = false;
	for (int i = start; i < end; i++) {
		bool digit = isdigit(token.str_start[i]);
		if (token.str_start[i] == '.') {
			if (seen_decimal) return false;
			seen_decimal = true;
		}
		else if (!digit)
			return false;
		any_digit |= digit;
	}
	*is_float = seen_decimal;
	return any_digit && (!back_had_float || seen_decimal);
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

	JUMP,
	JUMP_NEQ,

	CALL_USER_FUNC,
};

static bool is_integral_type(script_parameter_type type_) {
	return type_ != script_parameter_type::float_t;
}
static int parse_param_str(const char* s, bool* is_param_float)
{
	std::string str = s;
	int count = 0;
	for (int i = 0; i < str.size(); i++) {
		if (str[i] == 'i')
			is_param_float[count++] = false;
		else if (str[i] == 'f')
			is_param_float[count++] = true;

		assert(count < 11);
	}
	return count;
}

#include "StringUtil.h"

const char* BytecodeExpression::compile_new(BytecodeContext* global, ScriptVars_CFG* external_vars, const std::string& code) {

	std::string code_replaced = code;
	replace(code_replaced, "(", " ( ");
	replace(code_replaced, ")", " ) ");
	std::vector<StringView> tokens = to_tokens(code_replaced);
	std::reverse(tokens.begin(), tokens.end());
	return compile_from_tokens(global, external_vars, tokens);
}
const char* BytecodeExpression::compile_from_tokens(BytecodeContext* global, ScriptVars_CFG* vars,
	std::vector<StringView>& tokens)
{
	if (tokens.empty()) {
		throw LispError("Unexpected eof", nullptr);
	}
	StringView token = tokens.back();
	tokens.pop_back();


	if (token.cmp("(")) {
		const char* return_val = "";

		token = tokens.back();
		tokens.pop_back();

		if (token.cmp("not")) {
			auto type = compile_from_tokens(global, vars, tokens);

			if (strlen(type) != 1 || type[0] != 'i')
				throw LispError("not requires integral type", nullptr);

			push_inst(NOT);
			return_val = "i";
		}
		else if (token.cmp("or")) {
			auto type1 = compile_from_tokens(global, vars, tokens);
			auto type2 = compile_from_tokens(global, vars, tokens);

			if (strlen(type2) != 1 || strlen(type2) != 1 || type1[0] != 'i' || type2[0] != 'i')
				throw LispError("or requires integral type", nullptr);

			push_inst(OR);
			return_val = "i";
		}
		else if (token.cmp("and")) {
			auto type1 = compile_from_tokens(global, vars, tokens);
			auto type2 = compile_from_tokens(global, vars, tokens);

			if (strlen(type2) != 1 || strlen(type2) != 1 || type1[0] != 'i' || type2[0] != 'i')
				throw LispError("and requires integral type", nullptr);

			push_inst(AND);
			return_val = "i";
		}
		else if (token.cmp("if")) {
			int jump_eq = instructions.size();
			auto cond = compile_from_tokens(global, vars, tokens);
			if (strlen(cond) != 1 || cond[0] != 'i')
				throw LispError("expected int_t in 'if' condition", nullptr);
			push_inst(JUMP_NEQ);
			int jump_neq_fixup_ptr = instructions.size();
			push_4bytes(0);

			auto true_body = compile_from_tokens(global, vars, tokens);
			push_inst(JUMP);
			int jump_fixup_ptr = instructions.size();
			push_4bytes(0);

			int false_body_start = instructions.size();
			auto false_body = compile_from_tokens(global, vars, tokens);
			write_4bytes_at_location(false_body_start, jump_neq_fixup_ptr);

			if (strlen(false_body) != 1 || strlen(true_body) != 1)
				throw LispError("If body expects return value", nullptr);

			// cast one of the statements to float
			const char* out_type = true_body;
			if (true_body[0] != false_body[0]) {
				out_type = "f";
				if (false_body[0] == 'f') {
					// jump to end (false branch)
					push_inst(JUMP);
					int false_body_end_fixup_ptr = instructions.size();
					push_4bytes(0);
					// true jump enter:
					int true_jump_enter = instructions.size();
					write_4bytes_at_location(true_jump_enter, jump_fixup_ptr);
					// cast float (true branch)
					push_inst(CAST_0_F);
					// end:
					int end_loc = instructions.size();
					write_4bytes_at_location(end_loc, false_body_end_fixup_ptr);
				}
				else {
					// cast float (false branch)
					push_inst(CAST_0_F);
					// true-jump-enter:
					int true_jump_enter = instructions.size();
					write_4bytes_at_location(true_jump_enter, jump_fixup_ptr);
				}
			}
			else {
				int end_if_statement = instructions.size();
				write_4bytes_at_location(end_if_statement, jump_fixup_ptr);
			}

			return_val = out_type;
		}
		else if (token.cmp("func")) {

		}
		else {
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

			int math_op = 0;
			for (; math_op < 10; math_op++) {
				if (token.cmp(p[math_op].name)) {
					break;
				}
			}

			if (math_op < 10) {
				auto type1 = compile_from_tokens(global, vars, tokens);
				auto type2 = compile_from_tokens(global, vars, tokens);
				if (strlen(type1) != 1 || strlen(type2) != 1)
					throw LispError("Math ops only take 2 args", nullptr);

				bool isfloat = type1[0] == 'f' || type2[0] == 'f';
				if (isfloat && type1[0] == 'i')
					push_inst(CAST_1_F);
				if (isfloat && type2[0] == 'i')
					push_inst(CAST_0_F);

				if (isfloat)
					push_inst(p[math_op].op);
				else
					push_inst(p[math_op].op + 1);

				if (math_op >= 4) // math_op is a comparison operator (<,>,==,...)
					return_val = "i";
				else
					return_val = (isfloat) ? "f" : "i";
			}
			else {
				std::string str = token.to_stack_string().c_str();
				int user_func_idx = global->find_function_index(str);

				if (user_func_idx == -1)
					throw LispError("No op or func found", nullptr);

				auto user_func = global->get_function(user_func_idx);

				int in_arg_c = strlen(user_func->arg_in);
				int out_arg_c = strlen(user_func->arg_out);

				for (int i = 0; i < in_arg_c;) {
					if (tokens.empty() || tokens.back().cmp(")"))
						throw LispError("Too few args for function", nullptr);

					auto param_type = compile_from_tokens(global, vars, tokens);
					int count = strlen(param_type);
					if (i + count > in_arg_c)
						throw LispError("Too many args for user function", nullptr);

					for (int j = 0; j < count; j++) {
						if (user_func->arg_in[i + j] != param_type[j]) {
							if (param_type[j] != 'f')
								push_inst(CAST_0_F);
							else
								push_inst(CAST_0_I);
						}
					}
					i += count;

				}
				push_inst(CALL_USER_FUNC);
				push_4bytes(user_func_idx);
				return_val = user_func->arg_out;
			}
		}

		if (tokens.empty() || !tokens.back().cmp(")"))
			throw LispError("Expected )", nullptr);
		tokens.pop_back();

		return return_val;
	}
	else if (token.cmp(")")) {
		throw LispError("Unexpected )", nullptr);
	}
	else {
		if (token.str_len == 0)
			LispError("Empty token", nullptr);

		if (token.cmp("True") || token.cmp("true")) {
			push_inst(PUSH_CONST_I);
			push_4bytes(1);
			return "i";
		}
		if (token.cmp("False") || token.cmp("false")) {
			push_inst(PUSH_CONST_I);
			push_4bytes(0);
			return "i";
		}

		bool is_float = false;
		if (is_number(token, &is_float)) {
			if (is_float) {
				float f = atof(token.to_stack_string().c_str());
				push_inst(PUSH_CONST_F);
				push_4bytes(*(unsigned int*)&f);
				return "f";
			}
			else {
				int i = atoi(token.to_stack_string().c_str());
				push_inst(PUSH_CONST_I);
				push_4bytes(i);
				return"i";
			}
		}
		else {
			// find symbols
			std::string str = token.to_stack_string().c_str();
			const auto& find = vars->find(str);
			if (find.is_valid()) {
				auto type = vars->get_type(find);

				if (is_integral_type(type)) {
					push_inst(PUSH_I);
					push_4bytes(find.id);
					return "i";
				}
				else {
					push_inst(PUSH_F);
					push_4bytes(find.id);
					return "f";
				}
			}
			else {
				Parameter* p = global->find_constant(str);
				if (!p)
					throw LispError("undefined symbol", nullptr);
				if (is_integral_type(p->type)) {
					push_inst(PUSH_CONST_I);
					push_4bytes(p->ival);
					return "i";
				}
				else {
					push_inst(PUSH_CONST_F);
					push_4bytes(p->fval);
					return "f";
				}
			}
		}
	}
}

#define OPONSTACK(op, typein, typeout) stack[sp-2].typeout = stack[sp-2].typein op stack[sp-1].typein; sp-=1; break;
#define FLOAT_AND_INT_OP(opcode_, op) case opcode_: OPONSTACK(op, fval, fval); \
case (opcode_+1): OPONSTACK(op,ival,ival);
#define FLOAT_AND_INT_OP_OUTPUT_INT(opcode_, op) case opcode_: OPONSTACK(op,fval, ival); \
case (opcode_+1): OPONSTACK(op,ival, ival);

Parameter BytecodeExpression::execute(BytecodeContext* global, const ScriptVars_RT& vars) const
{
	static Parameter stack[64];
	int sp = 0;

	int count = instructions.size();

	for (int pc = 0; pc < count; pc++) {
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
			stack[sp - 1].ival = !stack[sp - 1].ival;
			break;
		case AND:
			stack[sp - 2].ival = stack[sp - 2].ival && stack[sp - 1].ival;
			sp -= 1;
			break;
		case OR:
			stack[sp - 2].ival = stack[sp - 2].ival || stack[sp - 1].ival;
			sp -= 1;
			break;

		case CAST_0_F:
			stack[sp - 1].fval = stack[sp - 1].ival;
			break;
		case CAST_0_I:
			stack[sp - 1].ival = stack[sp - 1].fval;
			break;
		case CAST_1_F:
			stack[sp - 2].fval = stack[sp - 2].ival;
			break;
		case CAST_1_I:
			stack[sp - 2].ival = stack[sp - 2].fval;
			break;

		case PUSH_CONST_F: {
			union {
				int i;
				float f;
			};
			i = read_4bytes(pc + 1);
			stack[sp++].fval = f;
			pc += 4;
		} break;
		case PUSH_CONST_I: {
			int i = read_4bytes(pc + 1);
			stack[sp++].ival = i;
			pc += 4;
		}break;
		case PUSH_F: {
			int index = read_4bytes(pc + 1);
			stack[sp++].fval = vars.get(handle<Parameter>{index}).fval;
			pc += 4;
		}break;
		case PUSH_I: {
			int index = read_4bytes(pc + 1);
			stack[sp++].ival = vars.get(handle<Parameter>{index}).ival;
			pc += 4;
		}break;


		case JUMP: {
			int where_ = read_4bytes(pc + 1);
			pc = where_ - 1;

		}break;
		case JUMP_NEQ: {
			int where_ = read_4bytes(pc + 1);
			int top = stack[--sp].ival;
			if (!top)
				pc = where_ - 1;
			else
				pc += 4;
		}break;
		case CALL_USER_FUNC: {
			int func_ = read_4bytes(pc + 1);
			auto funcdef = global->get_function(func_);
			BytecodeStack bs(stack, sp, 64);
			funcdef->ptr(bs);
			pc += 4;
			sp = bs.stack_ptr;
		}break;

		default:
			printf("!!! BYTECODE INVALID OP %d pc: %d !!!\n", op, pc);
			fflush(stdout);
			std::abort();
		}
	}
	assert(sp == 1);
	return stack[0];
}