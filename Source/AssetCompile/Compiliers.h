#pragma once

#include <cstdint>
#include <string>
class ModelImportSettings;
class Model;
class ModelCompilier
{
public:
	static bool compile(const char* name);

	static bool compile_from_settings(const std::string& output, ModelImportSettings* settings);
};

