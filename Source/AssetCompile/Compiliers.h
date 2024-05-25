#pragma once

#include <cstdint>
#include <string>

class Model;
class ModelCompilier
{
public:
	static bool compile(const char* name);
	static void compile_all();

	static bool load_gltf_model3(const std::string& filepath, Model* model);
};

class ImageCompilier
{
public:
	static bool compile(const char* name);

	static bool compile_from_text(const std::string& file, bool text_was_changed = false);

	static void compile_all();
};

class MaterialCompilier
{
public:
	static bool compile(const char* name);
	static void compile_all();
};

/*
mymaterial {

	color "mymaterial.png" 
	roughness "mymaterial_rough.png"
	metalness "mymaterial_metal.png"

	// then parser finds "mymaterial_rm.c_img" if rough or metal is found
	// "mymaterial_n.c_img"
	// "mymaterial_ao.c_img"



}


*/