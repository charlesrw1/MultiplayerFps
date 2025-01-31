#pragma once
#ifdef EDITOR_BUILD
#include <cstdint>
#include <string>
class ModelImportSettings;
class Model;
class ModelDefData;
class ModelCompilier
{
public:
	static bool compile(const char* name);

	static bool compile_from_settings(const std::string& output, ModelImportSettings* settings);
	static bool does_model_need_compile(const char* name, ModelDefData& def, bool needs_def);
};

#endif