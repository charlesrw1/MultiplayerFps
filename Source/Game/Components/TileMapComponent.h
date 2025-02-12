#pragma once
#include "Game/EntityComponent.h"
#include "Render/Texture.h"
#include "Game/SerializePtrHelpers.h"
#include "MiscEditors/DataClass.h"
#include <memory>
#include "Framework/Rect2d.h"
#include "Game/EntityPtr.h"


#ifdef EDITOR_BUILD
// this is just an editor definition, the actual display is made by the Generator subclass
struct TileDef {
	AssetPtr<Texture> texture;
	Rect2d rect{};
	Color32 color;
	const char* str = "";
};
struct LayerDef {
	std::vector<TileDef> defs;
};
#endif

struct TilemapData
{
	int width = 0;
	int height = 0;
	int layers = 0;
	std::vector<uint8_t> data;
};


class MeshBuilderComponent;
NEWCLASS(TileMapComponent, EntityComponent)
public:
	// raw data
	//REFLECT(name = "TilemapData", hide);
	//TilemapData data;
	// the generator
	REFLECT();
	float tile_size = 1.0;

	// for editor, if true, then it generates the objects for preview
	REFLECT(transient);
	bool show_built_map = false;

	virtual void generate() = 0;
#ifdef EDITOR_BUILD
	std::vector<EntityPtr> edGeneratedEntities;
	MeshBuilderComponent* edGridMeshbuilder = nullptr;
	std::vector<MeshBuilderComponent*> edMeshbuilderLayers;
	std::vector<LayerDef> edLayers;
#endif
};

// a simple 2d generator, uses a tilemap texture atlas
// specify the rows in pixels to align it
// creates a 2d meshbuilder
NEWCLASS(DefaultTileMap, TileMapComponent)
public:
	void pre_start() final {}
	void start() final {}
	void end() final {}
	void generate() final {}
	AssetPtr<Texture> texture;
	int rows_x = 0;
	int rows_y = 0;
	int total_count = 0;
};