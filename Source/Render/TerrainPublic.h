#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include "Framework/Handle.h"

class Texture;
class MaterialInstance;
class EditorTerrainAsset
{
public:
	// list of layers that can be drawn on (R,RGB,orRGBA)(dimensions) (cover whole play area)
	// list of outputs (material for blending) (R,RGB,or,RGBA)(dimensions)(coverwhole play area) (takes layers as inputs)
	
	// compute shader dispatch to make outputs

	// dispatch commands to draw to a layer (can be additive or blend, takes material)
	// invert layer etc.
	// masks are just defined as their own layer, used in material blend stage

	// non destructive brushes: takes a layer to draw to
	//	when transform changes, repaint everything in layer
	//	then repaint outputs
	struct OutputLayer {
		const MaterialInstance* generator = nullptr;
		const Texture* output_tex = nullptr;
	};
	std::vector<const Texture*> draw_layers;
	std::vector<OutputLayer> out_layers;

};

class CompiliedTerrainAsset
{
public:
	const Texture* assetptr_heightfield{};
	const MaterialInstance* assetptr_material{};
	uint32_t w{};
	uint32_t h{};
	// cooked heightfield
	// const PhysicsHeightfield* assetptr_heightfield{};
};


class TerrainInterfacePublic
{
public:
	virtual handle<CompiliedTerrainAsset> register_terrain(const CompiliedTerrainAsset* asset) = 0;
	virtual void update_terrain(handle<CompiliedTerrainAsset> asset) = 0;
	virtual void remove_terrain(handle<CompiliedTerrainAsset>& asset) = 0;

	virtual handle<EditorTerrainAsset> register_editor_terrain(const EditorTerrainAsset* asset) = 0;
	virtual void update_editor_terrain(handle<CompiliedTerrainAsset> asset) = 0;
	virtual void remove_editor_terrain(handle<EditorTerrainAsset>& asset) = 0;

	virtual void draw_brush_to_layer(
		handle<EditorTerrainAsset> asset,
		const MaterialInstance* mat, 
		bool additive, 
		glm::vec2 position,
		glm::vec2 size,
		float rotation,
		const Texture* layer) = 0;
	virtual void invert_layer(
		handle<EditorTerrainAsset> asset,
		const Texture* layer) = 0;
};