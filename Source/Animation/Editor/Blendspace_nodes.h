#pragma once
#include "Base_node.h"
#include "Basic_nodes.h"


struct Blendspace_Input {
	static const PropertyInfoList* get_props();
	float x = 0.0;
	float y = 0.0;
	std::string clip_name;
	Clip_Node_CFG* clip_node = nullptr;
};

CLASS_H_EXPLICIT_SUPER(Blendspace2d_EdNode, BaseNodeUtil_EdNode<BlendSpace2d_CFG>, Base_EdNode)

	MAKE_STANARD_SERIALIZE(Blendspace2d_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Blendspace 2D",
		BLEND_COLOR,
		"placeholder",
		1
	);


	char parameterization = 0;	// empty value

	std::vector<Blendspace_Input> blend_space_inputs;

	int serialized_index = -1;
};