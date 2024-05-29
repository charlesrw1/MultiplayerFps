#pragma once

#include <cstdint>
#include <string>

class Model;
class ModelCompilier
{
public:
	static bool compile(const char* name);

};

class ImageCompilier
{
public:
	static bool compile(const char* name);
	static bool compile_from_text(const std::string& file, bool text_was_changed = false);
};

class MaterialCompilier
{
public:
	static bool compile(const char* name);
};