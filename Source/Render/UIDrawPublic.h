#pragma once
#include <vector>
#include "DrawTypedefs.h"
#include "glm/glm.hpp"
class MaterialInstance;
class Texture;
struct UIDrawCall
{
	int index_start = 0;
	int index_count = 0;
	MaterialInstance* mat = nullptr;
	const Texture* texOverride = nullptr;
};
class MeshBuilder;
class RendererUIBackend
{
public:
	virtual void update(std::vector<UIDrawCall>& draw_calls_to_be_swapped, MeshBuilder& vertex_data, const glm::mat4& ViewProj) = 0;
};
extern RendererUIBackend* idrawUi;