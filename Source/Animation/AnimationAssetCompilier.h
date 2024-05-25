#pragma once
#include <vector>
#include <string>
#include "Framework/DictWriter.h"
class AnimationCompilier
{
public:
	static bool compile(
		const std::string& path, 
		DictWriter& writer,
		std::vector<uint8_t>& binary);
};

class ModelCompilier
{
	static bool compile(
		const std::string& recipe_path
	);	
};

enum class ImageCompileHint : uint8_t
{
	None,
	Standard,
	NormalMap,
};

class ImageCompilier
{
	static bool compile(
		const std::string& path,
		ImageCompileHint hint
	);
};