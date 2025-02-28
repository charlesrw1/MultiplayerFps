#pragma once
#include <vector>
#include "DrawTypedefs.h"

class MaterialInstance;
struct UIDrawCall
{
	int index_start = 0;
	int index_count = 0;
	program_handle shader{};
	MaterialInstance* mat = nullptr;
};
class MeshBuilder;
class RendererUIBackend
{
public:
	virtual void update(std::vector<UIDrawCall>& draw_calls_to_be_swapped, const MeshBuilder& vertex_data, const glm::mat4& ViewProj) = 0;
};