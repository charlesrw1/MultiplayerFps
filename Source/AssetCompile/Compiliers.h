#pragma once

#include <cstdint>
#include <string>

class Model;
class ModelCompilier
{
public:
	static bool compile(const char* name);

};


class MaterialCompilier
{
public:
	static bool compile(const char* name);

	static bool compile_new(const char* name);
};
