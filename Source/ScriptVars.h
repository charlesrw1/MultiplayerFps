#pragma once


#include "Util.h" // handle
#include "glm/glm.hpp"
#include <string>
#include <unordered_map>

#include "Parameter.h"

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
	script_parameter_type get_type(handle<Parameter> handle) const { return types.at(handle.id); }

	handle<Parameter> set(const char* str, script_parameter_type type) {
		types.push_back(type);
		name_to_index[str] = types.size() - 1;
	}

	std::vector<script_parameter_type> types;
	std::unordered_map<std::string, int> name_to_index;
};


struct ScriptVars_RT
{
	const Parameter& get(handle<Parameter> handle)const {
		return parameters[handle.id];
	}
	Parameter& get(handle<Parameter> handle) {
		static Parameter invalid;
		if (!handle.is_valid())
			return invalid;

		return parameters[handle.id];
	}
	std::vector<Parameter> parameters;
};

