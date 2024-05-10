#pragma once
#include <cstdint>
#include <vector>

struct script_value_t
{
	script_value_t() = default;
	explicit script_value_t(float f) : f(f) {}
	explicit script_value_t(uint32_t i) : ui32(i) {}
	explicit script_value_t(int i) : ui32(i) {}

	union {
		float f;		// float_t
		uint32_t ui32;
		uint64_t ui64;	// name_t, pointer_t, everything else
	};
};


// instance input variables, size must be > max(global_var_index) or crash
using program_script_vars_instance = std::vector<script_value_t>;
