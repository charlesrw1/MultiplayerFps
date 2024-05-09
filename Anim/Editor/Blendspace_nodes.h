#pragma once
#include "Base_node.h"
#include "Basic_nodes.h"


extern AutoEnumDef BlendSpace2dTopology_def;
enum class BlendSpace2dTopology
{
	FiveVert,
	NineVert,
	FifteenVert,
};

struct Blendspace_Input {
	static PropertyInfoList* get_props();
	float x = 0.0;
	float y = 0.0;
	std::string clip_name;
	Clip_Node_CFG* clip_node = nullptr;
};

class Blendspace2d_EdNode : public Base_EdNode
{
	EDNODE_HEADER(Blendspace2d_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Blendspace 2D",
		BLEND_COLOR,
		"placeholder",
		1
	);
	MAKE_STANDARD_INIT();

	bool compile_my_data()
	{
		return true;
	}


	char parameterization = 0;	// empty value
	BlendSpace2dTopology topology_2d = BlendSpace2dTopology::FiveVert;
	std::vector<Blendspace_Input> blend_space_inputs;

	int serialized_index = -1;
	BlendSpace2d_CFG* node = nullptr;
};