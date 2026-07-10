#pragma once
// Generic dynamic data-binding backing type shared by every Lua-created
// RmlUi data model. Lua tables are dynamically shaped, so instead of one
// C++ struct per document (the normal RmlUi RegisterStruct<T>() pattern),
// every model reuses this same pair of custom Rml::VariableDefinitions:
//   - RmlGenericRow: a name -> Rml::Variant map (a "cards" row's {image, text})
//   - std::vector<RmlGenericRow>: array of rows, drives `data-for`
// See RmlUiLua.cpp for the push-then-pull-on-demand bridge that writes into
// this storage and calls DataModelHandle::DirtyVariable in the same call.
#include <RmlUi/Core/DataVariable.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <unordered_map>
#include <vector>
#include <string>

struct RmlGenericRow {
	std::unordered_map<std::string, Rml::Variant> fields;
};

struct RmlGenericModel {
	std::string name;
	std::unordered_map<std::string, Rml::Variant> scalars;
	std::unordered_map<std::string, std::vector<RmlGenericRow>> arrays;
	Rml::DataModelHandle handle;
};

// Singleton VariableDefinitions, shared across every model/field - all state
// lives in the void* ptr RmlUi passes back in, not in these instances.
Rml::VariableDefinition* rmlui_generic_scalar_definition();
Rml::VariableDefinition* rmlui_generic_row_definition();
Rml::VariableDefinition* rmlui_generic_array_definition();
