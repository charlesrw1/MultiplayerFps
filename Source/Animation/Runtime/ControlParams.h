#pragma once


#include "glm/glm.hpp"
#include <string>
#include <unordered_map>
#include "Framework/ExpressionLang.h"
#include "Framework/EnumDefReflection.h"
#include "Framework/StringName.h"
#include "Framework/Handle.h"

// graph control parameter variables
// script layer variables


// if you modify this enum, change the AutoEnumDef!
extern AutoEnumDef control_param_type_def;
enum class control_param_type : uint8_t
{
	// integer types
	int_t,
	enum_t,
	bool_t,
	float_t,
};

struct AG_ControlParam
{
	std::string name;
	int default_i = 0;
	float default_f = 0.0;
	control_param_type type = control_param_type::int_t;
	int16_t enum_idx = 0;
	bool reset_after_tick = false;

	// if >0, then control param exists in anim instance, else it doesnt and this is a dangling reference
	int offset_in_anim_instance = -1;

	static PropertyInfoList* get_props();
};

struct ControlParam_CFG
{
	ControlParamHandle find(StringName name) const {
		return { find_no_handle(name) };
	}
	int find_no_handle(StringName name) const {
		const auto& find = name_to_index.find(name.get_hash());
		if (find != name_to_index.end()) {
			return { find->second };
		}
		return { -1 };
	}

	void set_float(program_script_vars_instance* rt, ControlParamHandle handle, float val) const {
		if (!handle.is_valid()) return;
		ASSERT(get_type(handle) == control_param_type::float_t);
		rt->at(handle.id).f = val;
	}
	void set_int(program_script_vars_instance* rt, ControlParamHandle handle, int val) const {
		if (!handle.is_valid()) return;
		ASSERT(get_type(handle) == control_param_type::int_t);
		rt->at(handle.id).ui32 = val;
	}
	void set_bool(program_script_vars_instance* rt, ControlParamHandle handle, bool val) const {
		if (!handle.is_valid()) return;
		ASSERT(get_type(handle) == control_param_type::bool_t);
		rt->at(handle.id).ui32 = val;
	}

	void set_float_nh(program_script_vars_instance* rt, int handle, float val) const {
		set_float(rt, { handle }, val);
	}
	void set_int_nh(program_script_vars_instance* rt, int handle, int val) const {
		set_int(rt, { handle }, val);
	}
	void set_bool_nh(program_script_vars_instance* rt, int handle, bool val) const {
		set_bool(rt, { handle }, val);
	}

	float get_float(program_script_vars_instance* rt, ControlParamHandle handle) const {
		if (!handle.is_valid()) return 0.f;
		ASSERT(get_type(handle) == control_param_type::float_t);
		return rt->at(handle.id).f;
	}
	int get_int(program_script_vars_instance* rt, ControlParamHandle handle) const {
		if (!handle.is_valid()) return 0;
		ASSERT(get_type(handle) == control_param_type::int_t);
		return rt->at(handle.id).ui32;
	}
	bool get_bool(program_script_vars_instance* rt, ControlParamHandle handle) const {
		if (!handle.is_valid()) return 0;
		ASSERT(get_type(handle) == control_param_type::bool_t);
		return rt->at(handle.id).ui32;
	}


	control_param_type get_type(ControlParamHandle handle) const {
		ASSERT(handle.id >= 0 && handle.id < types.size());
		return types[handle.id].type;
	}

	static script_types get_script_type_for_control_param(control_param_type param) {
		switch (param)
		{
		case control_param_type::int_t:return script_types::int_t;
		case control_param_type::enum_t:return script_types::int_t;
		case control_param_type::bool_t:return script_types::bool_t;
		case control_param_type::float_t:return script_types::float_t;
		default:
			ASSERT(0);
		}
	}

	void clear_variables() {
		name_to_index.clear();
		types.clear();
	}
	ControlParamHandle push_variable(const AG_ControlParam& param) {
		types.push_back(param);
		name_to_index[StringName(param.name.c_str()).get_hash()] = (int)types.size() - 1;
		return { (int)types.size() - 1 };
	}
	void set_library_vars(Library* lib);

	static PropertyInfoList* get_props();

	std::vector<AG_ControlParam> types;
	std::unordered_map<name_hash_t, int> name_to_index;
};