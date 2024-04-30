#pragma once


#include "Util.h" // handle
#include "glm/glm.hpp"
#include <string>
#include <unordered_map>

#include "Parameter.h"

struct Parameter_CFG
{
	Parameter default_;
	bool reset_after_tick = false;
};

struct ScriptVars_CFG
{
	handle<Parameter> find(const std::string& str) const {
		const auto& find = name_to_index.find(str);
		if (find != name_to_index.end()) {
			handle<Parameter> h;
			h.id = find->second;
			return h;
		}
		return { -1 };
	}
	script_parameter_type get_type(handle<Parameter> handle) const { return types.at(handle.id).default_.type; }

	handle<Parameter> set(const char* str, script_parameter_type type) {
		Parameter_CFG param;
		param.default_.init_for_type(type);
		param.reset_after_tick = false;

		types.push_back(param);
		name_to_index[str] = types.size() - 1;

		return { (int)types.size() - 1};
	}

	std::vector<Parameter_CFG> types;
	std::unordered_map<std::string, int> name_to_index;
};


struct ScriptVars_RT
{
	const Parameter& get(handle<Parameter> handle)const {
		return parameters[handle.id];
	}
	Parameter& get(handle<Parameter> handle) {
		return parameters[handle.id];
	}

	void set_float(handle<Parameter> handle, float f) {
		if (handle.is_valid()) {
			auto p = get(handle);
			ASSERT(p.type == script_parameter_type::float_t);
			p.fval = f;
		}
	}
	void set_int(handle<Parameter> handle, int i) {
		if (handle.is_valid()) {
			auto p = get(handle);
			ASSERT(p.type == script_parameter_type::int_t);
			p.ival = i;
		}
	}
	void set_bool(handle<Parameter> handle, bool b) {
		if (handle.is_valid()) {
			auto p = get(handle);
			ASSERT(p.type == script_parameter_type::bool_t);
			p.ival = b;
		}
	}


	std::vector<Parameter> parameters;
};

