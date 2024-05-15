#include "Framework/ExpressionLang.h"
#include "Framework/Handle.h"

#include "Framework/StringUtil.h"
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

std::vector<StringViewAndLine> to_tokens(const std::string& input)
{
	std::vector<StringViewAndLine> out;

	int start = 0;
	int count = 0;
	int line = 0;

	for (int idx = 0; idx < input.size(); idx++) {
		char c = input[idx];

		if (c == ' ' || c == '\t' || c == '\n') {
			if (count != 0) {
				out.push_back(StringViewAndLine(input.data() + start, count, line));
			}
			if (c == '\n') line++;
			count = 0;
		}
		else {
			if (count == 0)
				start = idx;
			count++;
		}
	}
	if (count != 0)
		out.push_back(StringViewAndLine(input.data() + start, count, line));
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

	NOT,
	PUSH_CONST_F,
	PUSH_CONST_I,
	PUSH_F,
	PUSH_I,
	PUSH_CONST_INT64,
	PUSH_STACK,			// @arg = index

	STORE_STACK,

	CAST_0_F,
	CAST_0_I,
	CAST_1_F,
	CAST_1_I,

	LOAD_REG,

	SET_SP,
	JUMP,

	JUMP_NEQ,
	JUMP_EQ,

	POP_OR_JUMP_NEQ,
	POP_OR_JUMP_EQ,

	CALL_USER_FUNC,
};

struct opcode_info {
	const char* name;
	int args = 0;
};

const static opcode_info opcode_infos[] = {
	{"ADD_F", 0 },
	{"ADD_I", 0 },
	{"SUB_F", 0 },
	{"SUB_I", 0 },
	{"MULT_F", 0 },
	{"MULT_I", 0 },
	{"DIV_F", 0 },
	{"DIV_I", 0 },
	{"LT_F", 0 },
	{"LT_I", 0 },
	{"LEQ_F", 0 },
	{"LEQ_I", 0 },
	{"GT_F", 0 },
	{"GT_I", 0 },
	{"GEQ_F", 0 },
	{"GEQ_I", 0 },
	{"EQ_F", 0 },
	{"EQ_I", 0 },
	{"NOTEQ_F", 0 },
	{"NOTEQ_I", 0 },

	{"NOT", 0 },

	{"PUSH_CONST_F", 1 },
	{"PUSH_CONST_I", 1 },
	{"PUSH_F", 1 },
	{"PUSH_I", 1 },
	{"PUSH_CONST_INT64", 2 },
	{"PUSH_STACK", 1 },
	{"STORE_STACK", 1 },

	{"CAST_0_F", 0 },
	{"CAST_0_I", 0 },
	{"CAST_1_F", 0 },
	{"CAST_1_I", 0 },

	{"LOAD_REG", 0 },

	{"SET_SP", 0 },
	{"JUMP", 1 },
	{"JUMP_NEQ", 1 },
	{"JUMP_EQ", 1 },

	{"POP_OR_JUMP_NEQ", 1 },
	{"POP_OR_JUMP_EQ", 1 },

	{"CALL_USER_FUNC", 1 },
};


static std::string stringview_to_string(StringView view)
{
	return std::string(view.str_start, view.str_len);
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


static void parse_type_string(const char* s, int line, compile_result& res, const Program* prog);
static void add_type(StringView str, int line, compile_result& res, const Program* prog)
{
	int start = res.out_types.size();
	if (str.str_len == 0) {
		res.out_types.push_back(script_types::empty_t);
		return;
	}
	else if (str.str_len == 1) {
		switch (str.str_start[0]) {
		case 'f': res.out_types.push_back(script_types::float_t); return;
		case 'i': res.out_types.push_back(script_types::int_t); return;
		case 'n': res.out_types.push_back(script_types::name_t); return;
		case 'b': res.out_types.push_back(script_types::bool_t); return;
		case 'e': res.out_types.push_back(script_types::empty_t); return;
		case 'p': res.out_types.push_back(script_types::pointer_t); return;
		}
	}
	else if (str.cmp("float"))
		res.out_types.push_back(script_types::float_t);
	else if (str.cmp("int"))
		res.out_types.push_back(script_types::int_t);
	else if (str.cmp("name"))
		res.out_types.push_back(script_types::name_t);
	else if (str.cmp("bool"))
		res.out_types.push_back(script_types::bool_t);
	else if (str.cmp("ptr"))
		res.out_types.push_back(script_types::pointer_t);
	else if (str.cmp("empty"))
		res.out_types.push_back(script_types::empty_t);

	if (res.out_types.size() > start)
		return;

	uint64_t hash = StringName(str.to_stack_string().c_str()).get_hash();
	Program::find_tuple find = prog->find_def(hash);

	if (!find.def)
		throw CompileError("cant find type: " + stringview_to_string(str), line);
	if (find.def->type != script_def_type::struct_t)
		throw CompileError("type name is not defined as struct: " + stringview_to_string(str), line);

	res.out_types.push_back(script_types::custom_t);
	res.custom_types.push_back(find.full_index);
}

static void parse_type_string(const char* str, int line, compile_result& res, const Program* prog, int string_len = 5000)
{
	res.out_types.resize(0);
	res.custom_types.resize(0);
	int count = 0;
	int start = 0;
	int i = 0;
	bool next_comma = false;
	const char* s = str;
	for (; *s != 0 && i < string_len; i++, s++) {
		if (*s == ',') {
			add_type(StringView(str + start, count), line, res, prog);
			count = 0;
			continue;
		}
		else if (*s == ' ') {
			if (count != 0) {
				throw CompileError("cant have spaces in type string: " + std::string(str + start, count), line);
			}
		}
		if (count == 0)
			start = i;
		count++;
	}

	add_type(StringView(str + start, count), line, res, prog);
}

static void parse_type_from_tokens(std::vector<StringViewAndLine>& toks, compile_result& res, const Program* prog)
{
	res.clear();
	if (toks.empty()) throw CompileError("empty type string", -1);
	StringView front = toks.back().view;
	int line_num = toks.back().line_num;
	toks.pop_back();
	if (front.cmp("[")) {
		if (toks.empty()) throw CompileError("expected statement after [", line_num);
		front = toks.back().view;
		const char* start = front.str_start;
		while (!front.cmp("]")) {
			if (toks.empty()) throw CompileError("empty", line_num);
			front = toks.back().view;
		}
		const char* end = front.str_start;
		int count = end - start;
		parse_type_string(start, line_num, res, prog, count);
		toks.pop_back();
		return;
	}
	parse_type_string(front.str_start, line_num, res, prog, front.str_len);

}

#include "Framework/StringUtil.h"

compile_result BytecodeExpression::compile(const Program* global, const std::string& code, StringName selftype) {
	instructions.clear();

	std::string code_replaced = code;
	replace(code_replaced, "(", " ( ");
	replace(code_replaced, ")", " ) ");
	replace(code_replaced, ":", " : ");
	replace(code_replaced, "->", " -> ");
	replace(code_replaced, ",", " , ");
	replace(code_replaced, "[", " [ ");
	replace(code_replaced, "]", " ] ");



	std::vector<StringViewAndLine> tokens = to_tokens(code_replaced);
	std::reverse(tokens.begin(), tokens.end());

	compile_result res;
	compile_data_referenced refed;

	while (!tokens.empty())
		compile_from_tokens(res, refed, 0, global, tokens, selftype);

	return res;
}

static bool not_integer_or_bool_type(compile_result& res)
{
	return (res.out_types.size() != 1 || (res.out_types[0] != script_types::bool_t && res.out_types[0] != script_types::int_t));
}

static bool not_bool_type(compile_result& res)
{
	return (res.out_types.size() != 1 || (res.out_types[0] != script_types::bool_t));
}

static bool is_int_or_float_type(compile_result& res)
{
	return (res.out_types.size() == 1 &&
		(res.out_types[0] == script_types::bool_t
			|| res.out_types[0] == script_types::float_t
			|| res.out_types[0] == script_types::int_t));
}
static bool is_float_type(compile_result& res)
{
	return (res.out_types.size() == 1 &&
		res.out_types[0] == script_types::float_t);
}



static bool ensure_types_are_equal(const compile_result& res1, const compile_result& res2)
{
	if (res1.out_types.size() != res2.out_types.size()) return false;
	if (res1.custom_types.size() != res2.custom_types.size()) return false;
	for (int i = 0; i < res1.out_types.size(); i++) {
		if (res1.out_types[i] != res2.out_types[i]) return false;
	}
	for (int i = 0; i < res1.custom_types.size(); i++) {
		if (res1.custom_types[i] != res2.custom_types[i]) return false;
	}
	return true;
}

static bool has_quotations(StringView view)
{
	if (view.str_len < 2) return false;
	bool start = view.str_start[0] == '\"' || view.str_start[0] == '\'';
	bool end = view.str_start[view.str_len - 1] == '\"' || view.str_start[view.str_len - 1] == '\'';
	return start && end;
}


void BytecodeExpression::compile_from_tokens(compile_result& res, compile_data_referenced& local, int base_ptr, const  Program* global,
	std::vector<StringViewAndLine>& tokens, StringName selftype)
{
	res.custom_types.resize(0);
	res.out_types.resize(0);

	if (tokens.empty()) {
		throw CompileError("Unexpected eof", -1);
	}

	int line_num = tokens.back().line_num;
	StringView token = tokens.back().view;
	tokens.pop_back();


	if (token.cmp("(")) {
		compile_result out_result;

		line_num = tokens.back().line_num;
		token = tokens.back().view;
		tokens.pop_back();

		if (token.cmp("not")) {
			compile_from_tokens(res, local, base_ptr, global, tokens, selftype);

			// only integer type allowed
			if (not_integer_or_bool_type(res))
				throw CompileError("not requires bool/int type", line_num);

			push_inst(NOT);

			out_result.push_std_type(script_types::bool_t);
		}
		else if (token.cmp("or")) {
			compile_from_tokens(res, local, base_ptr, global, tokens, selftype);
			if (not_integer_or_bool_type(res))
				throw CompileError("or requires bool/int type (arg1)", line_num);

			push_inst(POP_OR_JUMP_NEQ);
			int first_ofs = instructions.size();
			push_4bytes(0);

			compile_from_tokens(res, local, base_ptr, global, tokens, selftype);
			if (not_integer_or_bool_type(res))
				throw CompileError("or requires bool/int type (arg2)", line_num);

			int where_ = instructions.size();
			write_4bytes_at_location(where_, first_ofs);

			out_result.push_std_type(script_types::bool_t);
		}
		else if (token.cmp("and")) {
			compile_from_tokens(res, local, base_ptr, global, tokens, selftype);
			if (not_integer_or_bool_type(res))
				throw CompileError("and requires bool/int type (arg1)", line_num);

			push_inst(POP_OR_JUMP_EQ);
			int first_ofs = instructions.size();
			push_4bytes(0);

			compile_from_tokens(res, local, base_ptr, global, tokens, selftype);
			if (not_integer_or_bool_type(res))
				throw CompileError("and requires bool/int type (arg2)", line_num);


			int where_ = instructions.size();
			write_4bytes_at_location(where_, first_ofs);

			out_result.push_std_type(script_types::bool_t);
		}
		else if (token.cmp("?")) {
			int jump_eq = instructions.size();

			compile_from_tokens(res, local, base_ptr, global, tokens, selftype);
			if (not_bool_type(res))
				throw CompileError("if requires bool condition", line_num);

			push_inst(JUMP_NEQ);
			int jump_neq_fixup_ptr = instructions.size();
			push_4bytes(0);

			compile_from_tokens(res, local, base_ptr, global, tokens, selftype);

			push_inst(JUMP);
			int jump_fixup_ptr = instructions.size();
			push_4bytes(0);

			int false_body_start = instructions.size();

			compile_result res_false;

			compile_from_tokens(res_false, local, base_ptr, global, tokens, selftype);
			write_4bytes_at_location(false_body_start, jump_neq_fixup_ptr);

			if (!ensure_types_are_equal(res, res_false))
				throw CompileError("if statement requires both bodies return same type(s)", line_num);


			int end_if_statement = instructions.size();
			write_4bytes_at_location(end_if_statement, jump_fixup_ptr);

			out_result = res;
		}
		else if (token.cmp("struct")) {

		}
		else if (token.cmp("func")) {

		}
		else if (token.cmp("=")) {

			token = tokens.back().view;
			tokens.pop_back();
			auto find = local.find_local(token.to_stack_string().c_str());

			if (!find) throw CompileError("no local variable with name", line_num);

			compile_from_tokens(res, local, base_ptr, global, tokens, selftype);

			if (!ensure_types_are_equal(res, find->type))
				throw CompileError("local var type doesnt match assignment", line_num);


			push_inst(STORE_STACK);
			uint32_t dat = find->offset;
			dat |= find->type.out_types.size() << 16;
			push_4bytes(dat);

			res.clear();
			res.push_std_type(script_types::empty_t);
		}
		else if (token.cmp("let")) {

			if (tokens.size() < 1)
				throw CompileError("missing statement after let", line_num);

			token = tokens.back().view;
			tokens.pop_back();

			compile_local_variable clv;
			clv.name = token.to_stack_string().c_str();
			clv.view = token;
			clv.offset = base_ptr;

			// optional type hint
			if (tokens.size() < 2)
				throw CompileError("missing local var decleration", line_num);

			token = tokens.back().view;
			tokens.pop_back();

			compile_result type_hint_res;
			bool has_type_hint = false;
			if (token.cmp(":")) {
				has_type_hint = true;
				parse_type_from_tokens(tokens, type_hint_res, global);
			}
			else {
				tokens.push_back(StringViewAndLine(token.str_start, token.str_len, line_num));
			}

			compile_from_tokens(res, local, base_ptr, global, tokens, selftype);

			if (has_type_hint && !ensure_types_are_equal(res, type_hint_res))
				throw CompileError("type hint mismatches assignment", line_num);
			if (res.out_types.size() == 0 || res.out_types[0] == script_types::empty_t)
				throw CompileError("cant assign empty to local var", line_num);

			clv.type = res;
			local.vars.push_back(compile_local_variable(clv));

			res.clear();
			res.out_types.push_back(script_types::empty_t);

			base_ptr += res.out_types.size();
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
				compile_from_tokens(res, local, base_ptr, global, tokens, selftype);
				if (!is_int_or_float_type(res))
					throw CompileError("math op requires numeric type", line_num);

				compile_result type2_res;
				compile_from_tokens(type2_res, local, base_ptr, global, tokens, selftype);
				if (!is_int_or_float_type(res))
					throw CompileError("math op requires numeric type", line_num);

				bool isfloat = is_float_type(res) || is_float_type(type2_res);
				if (isfloat && !is_float_type(res))
					push_inst(CAST_1_F);
				if (isfloat && !is_float_type(type2_res))
					push_inst(CAST_0_F);

				if (isfloat)
					push_inst(p[math_op].op);
				else
					push_inst(p[math_op].op + 1);

				if (math_op >= 4) // math_op is a comparison operator (<,>,==,...)
					out_result.push_std_type(script_types::bool_t);
				else
					(isfloat) ? out_result.push_std_type(script_types::float_t) : out_result.push_std_type(script_types::int_t);
			}
			else {
				std::string str = token.to_stack_string().c_str();
				Program::find_tuple find = global->find_def(str.c_str());

				if (!find.def || find.def->type != script_def_type::nativefunc_t)
					throw CompileError("user function not defined: " + str, line_num);

				compile_result in_arg_types;
				parse_type_string(find.def->func.argin, line_num, in_arg_types, global, 5000);

				int custom_idx = 0;
				int in_argc = in_arg_types.out_types.size();

				for (int i = 0; i < in_argc;) {
					if (in_arg_types.out_types[i] == script_types::empty_t) {
						i++;
						continue;
					}

					if (tokens.empty() || tokens.back().view.cmp(")"))
						throw CompileError("Too few args for function", line_num);

					compile_from_tokens(res, local, base_ptr, global, tokens, selftype);
					int count = res.out_types.size();
					if (i + count > in_argc)
						throw CompileError("Too many args for user function", line_num);

					int actual_in_struct_idx = 0;
					for (int j = 0; j < count; j++) {
						auto type_expected = in_arg_types.out_types[i + j];
						auto type_got = res.out_types[j];

						if (type_expected == type_got) {
							if (type_expected == script_types::custom_t) {
								assert(custom_idx < in_arg_types.custom_types.size() && actual_in_struct_idx < res.custom_types.size());
								if (in_arg_types.custom_types[custom_idx++] == res.custom_types[actual_in_struct_idx++])
									continue;
							}
							continue;
						}
						else if (count == 1) {
							if (is_int_or_float_type(in_arg_types) && is_int_or_float_type(res)) {

								if (is_float_type(in_arg_types) != is_float_type(res)) {
									if (is_float_type(res))
										push_inst(CAST_0_I);
									else
										push_inst(CAST_0_F);
								}

								continue;
							}
						}

						throw CompileError("Wrong type", line_num);
					}
					i += count;

				}
				push_inst(CALL_USER_FUNC);
				push_4bytes(find.full_index);

				parse_type_string(find.def->func.argout, line_num, out_result, global, 5000);// writes to out_result
			}
		}
		if (tokens.empty() || !tokens.back().view.cmp(")")) {
			throw CompileError("Expected )", line_num);
		}
		res = out_result;

		tokens.pop_back();
	}
	else if (token.cmp(")")) {
		tokens.push_back(StringViewAndLine(token.str_start, token.str_len, line_num));
		res.clear();
		res.push_std_type(script_types::empty_t);
		return;
	}
	else {
		res.clear();
		if (token.str_len == 0)
			throw CompileError("Empty string (?)", line_num);


		if ((token.str_start[0] == 'n' && has_quotations(StringView(token.str_start + 1, token.str_len - 1)))
			|| has_quotations(token)) {

			StringView view = (token.str_start[0] == 'n') ?
				StringView(token.str_start + 2, token.str_len - 3) :
				StringView(token.str_start + 1, token.str_len - 2);


			name_hash_t hash = StringName(stringview_to_string(view).c_str()).get_hash();
			push_inst(PUSH_CONST_INT64);
			push_4bytes((uint32_t)hash);
			push_4bytes(uint32_t(hash >> 32));
			res.push_std_type(script_types::name_t);
			return;
		}

		if (token.cmp("True") || token.cmp("true")) {
			push_inst(PUSH_CONST_I);
			push_4bytes(1);
			res.push_std_type(script_types::bool_t);
			return;
		}
		if (token.cmp("False") || token.cmp("false")) {
			push_inst(PUSH_CONST_I);
			push_4bytes(0);
			res.push_std_type(script_types::bool_t);
			return;
		}

		bool is_float = false;
		if (is_number(token, &is_float)) {
			if (is_float) {
				float f = atof(token.to_stack_string().c_str());
				push_inst(PUSH_CONST_F);
				push_4bytes(*(unsigned int*)&f);
				res.push_std_type(script_types::float_t);
			}
			else {
				int i = atoi(token.to_stack_string().c_str());
				push_inst(PUSH_CONST_I);
				push_4bytes(i);
				res.push_std_type(script_types::int_t);
			}
		}
		else {
			res.clear();
			// find symbols
			std::string str = token.to_stack_string().c_str();
			if (str == "self") {

				Program::find_tuple find = global->find_def(selftype);
				if (!find.def || find.def->type != script_def_type::struct_t)
					throw CompileError("cant find selftype", line_num);
				res.push_custom_type(find.full_index);
				return;
			}


			const compile_local_variable* local_var = nullptr;
			local_var = local.find_local(str.c_str());
			if (local_var) {

				push_inst(PUSH_STACK);

				uint32_t dat = local_var->offset;
				dat |= ((uint32_t)local_var->type.out_types.size()) << 16;

				push_4bytes(dat);
				res = local_var->type;
				return;
			}
			Program::find_tuple find = global->find_def(str.c_str());

			if (!find.def || (find.def->type != script_def_type::constant_t && find.def->type != script_def_type::global_t))
				throw CompileError("unknown global variable or constant: " + str, line_num);

			if (find.def->type == script_def_type::constant_t) {
				auto def = find.def;
				assert(def->constant.type == script_types::bool_t || def->constant.type == script_types::int_t || def->constant.type == script_types::float_t);
				if (def->constant.type != script_types::float_t) {
					push_inst(PUSH_I);
					push_4bytes(def->constant.value.ui64);
				}
				else {
					push_inst(PUSH_F);
					push_4bytes(def->constant.value.ui64);
				}
				res.push_std_type(def->constant.type);
			}
			else if (find.def->type == script_def_type::global_t) {
				auto def = find.def;
				assert(def->type == script_def_type::global_t);
				assert(def->global.type == script_types::bool_t || def->global.type == script_types::int_t || def->global.type == script_types::float_t);

				if (def->global.type != script_types::float_t) {
					push_inst(PUSH_I);
				}
				else {
					push_inst(PUSH_F);
				}
				push_4bytes(find.full_index);
				res.push_std_type(def->global.type);
			}
			else
				assert(0);
		}
	}
}

#define OPONSTACK(op, typein, typeout) stack[sp-2].typeout = stack[sp-2].typein op stack[sp-1].typein; sp-=1; break;
#define FLOAT_AND_INT_OP(opcode_, op) case opcode_: OPONSTACK(op, f, f); \
case (opcode_+1): OPONSTACK(op,ui32,ui32);
#define FLOAT_AND_INT_OP_OUTPUT_INT(opcode_, op) case opcode_: OPONSTACK(op,f, ui32); \
case (opcode_+1): OPONSTACK(op,ui32, ui32);

#define CAST_OP(op, down, in, out) case op: stack[sp - down].out = stack[sp - down].in; break;

void BytecodeExpression::execute(script_state* state, const Program* prog, program_script_vars_instance* input) const
{
	script_value_t* stack = state->stack;
	int sp = state->stack_ptr;

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

			CAST_OP(CAST_0_F, 1, ui32, f);
			CAST_OP(CAST_0_I, 1, f, ui32);
			CAST_OP(CAST_1_F, 2, ui32, f);
			CAST_OP(CAST_1_I, 2, f, ui32);

		case NOT:
			stack[sp - 1].ui32 = !stack[sp - 1].ui32;
			break;


		case PUSH_STACK: {
			uint32_t dat = read_4bytes(pc + 1);
			uint32_t ofs = dat & 0xffff;
			uint32_t count = dat >> 16;
			memcpy(&stack[sp], &stack[ofs], count * sizeof(script_value_t));
			sp += count;
			pc += 4;
		} break;
		case STORE_STACK: {
			uint32_t dat = read_4bytes(pc + 1);
			uint32_t ofs = dat & 0xffff;
			uint32_t count = dat >> 16;
			memcpy(&stack[ofs], &stack[sp - count], count * sizeof(script_value_t));
			sp -= count;
			pc += 4;
		} break;

		case PUSH_CONST_F: {
			union {
				uint32_t i;
				float f;
			};
			i = read_4bytes(pc + 1);
			stack[sp++].f = f;
			pc += 4;
		} break;
		case PUSH_CONST_I: {
			uint32_t i = read_4bytes(pc + 1);
			stack[sp++].ui32 = i;
			pc += 4;
		}break;
		case PUSH_CONST_INT64: {
			uint64_t low = read_4bytes(pc + 1);
			uint64_t high = read_4bytes(pc + 5);
			stack[sp++].ui64 = low | (high << 32);
			pc += 8;
		}break;
		case PUSH_F: {
			int index = read_4bytes(pc + 1);
			assert(index >= 0 && index < input->size());
			stack[sp++].f = input->at(index).f;
			pc += 4;
		}break;
		case PUSH_I: {
			int index = read_4bytes(pc + 1);
			assert(index >= 0 && index < input->size());
			stack[sp++].ui32 = input->at(index).ui32;
			pc += 4;
		}break;


		case JUMP: {
			int where_ = read_4bytes(pc + 1);
			pc = where_ - 1;

		}break;
		case JUMP_NEQ: {
			int where_ = read_4bytes(pc + 1);
			int top = stack[--sp].ui32;
			if (!top)
				pc = where_ - 1;
			else
				pc += 4;
		}break;

		case POP_OR_JUMP_EQ: {
			int where_ = read_4bytes(pc + 1);
			int top = stack[sp - 1].ui32;
			if (top == 0) {
				pc = where_ - 1;
			}
			else {
				--sp;
				pc += 4;
			}
		}break;

		case POP_OR_JUMP_NEQ: {
			int where_ = read_4bytes(pc + 1);
			int top = stack[sp-1].ui32;
			if (top != 0) {
				pc = where_ - 1;
			}
			else {
				--sp;
				pc += 4;
			}
		}break;

		case CALL_USER_FUNC: {
			int func_ = read_4bytes(pc + 1);
			assert(func_ >= 0 && func_ < prog->func_ptrs.size());
			state->stack_ptr = sp;
			auto& funcdef = prog->func_ptrs.at(func_);
			funcdef.function(state);
			pc += 4;
			sp = state->stack_ptr;
		}break;

		default:
			printf("!!! BYTECODE INVALID OP %d pc: %d !!!\n", op, pc);
			fflush(stdout);
			std::abort();
		}
	}

	state->stack_ptr = sp;
}

const script_variable_def* Library::find_def(StringName name) const
{
	auto find = name_to_idx.find(name.get_hash());
	if (find == name_to_idx.end()) return nullptr;
	return defs.data() + find->second;
}

void BytecodeExpression::print_instructions()
{
	int num_instructions = 0;
	for (int i = 0; i < instructions.size(); i++) {
		const opcode_info& inf = opcode_infos[instructions[i]];
		num_instructions++;
		i += 4 * inf.args;
	}
	printf("Instructions %d (bytes %d)\n", num_instructions, (int)instructions.size());
	for (int i = 0; i < instructions.size(); i++) {
		const opcode_info& inf = opcode_infos[instructions[i]];
		printf("0x%02x ", i);
		printf("%-20s", inf.name);
		for (int j = 0; j < inf.args; j++) {
			uint32_t t = read_4bytes(i + 1 + 4 * j);
			printf("0x%08x ", t);
		}
		printf("\n");

		i += 4 * inf.args;
	}

}

void Library::push_global_def(const char* name, script_types type) {
	script_variable_def def;
	def.name = name;
	def.type = script_def_type::global_t;
	def.global.type = type;
	def.global.index = num_vars++;
	def.global.name_hash = StringName(name).get_hash();
	name_to_idx[StringName(name).get_hash()] = defs.size();
	defs.push_back(def);
}

void Library::push_function_def(const char* name, const char* out, const char* in, bytecode_function func)
{
	script_variable_def def;
	def.name = name;
	def.type = script_def_type::nativefunc_t;
	def.func.argin = in;
	def.func.argout = out;
	def.func.index = num_funcs++;
	def.func.ptr = func;

	name_to_idx[StringName(name).get_hash()] = defs.size();
	defs.push_back(def);
}

void Library::push_constant_def(const char* name, script_types type, script_value_t value)
{
	script_variable_def def;
	def.name = name;
	def.type = script_def_type::constant_t;
	def.constant.type = type;
	def.constant.value = value;

	name_to_idx[StringName(name).get_hash()] = defs.size();
	defs.push_back(def);
}

void Library::push_struct_def(const char* name, const char* fields)
{
	script_variable_def def;
	def.name = name;
	def.type = script_def_type::struct_t;
	def.struct_.fields = fields;
	def.struct_.index = num_structs++;

	name_to_idx[StringName(name).get_hash()] = defs.size();
	defs.push_back(def);
}

void Program::push_library(const Library* lib)
{
	full_import import_;
	import_.lib = lib;
	if (imports.size() > 0) {
		full_import* back = &imports[imports.size() - 1];
		import_.var_start = back->lib->num_vars + back->var_start;
		import_.func_start = back->lib->num_funcs + back->func_start;
		import_.struct_start = back->lib->num_structs + back->struct_start;
	}
	else
		import_.func_start = import_.var_start = 0;

	imports.push_back(import_);
	int pre_size = func_ptrs.size();
	for (int i = 0; i < lib->defs.size(); i++) {
		if (lib->defs[i].type == script_def_type::nativefunc_t) {
			runtime_func_ptr rt;
			rt.function = lib->defs[i].func.ptr;
#ifdef DEBUG_SCRIPT
			rt.lib = lib;
			rt.def = lib->defs.data() + i;
#endif
			func_ptrs.push_back(rt);
		}
	}
	int post_size = func_ptrs.size();
	assert(post_size - pre_size == lib->num_funcs);
}

Program::find_tuple Program::find_def(StringName name) const
{
	int start = imports.size() - 1;
	for (; start >= 0; start--) {	// search in reverse order
		const full_import* imp = imports.data() + start;
		const script_variable_def* def = imp->lib->find_def(name);
		if (def) {
			find_tuple tup;
			tup.def = def;
			if (def->type == script_def_type::nativefunc_t)
				tup.full_index = imp->func_start + def->func.index;
			else if (def->type == script_def_type::global_t)
				tup.full_index = imp->var_start + def->global.index;
			else if (def->type == script_def_type::struct_t)
				tup.full_index = imp->struct_start + def->struct_.index;

			return tup;
		}
	}

	return find_tuple();
}
