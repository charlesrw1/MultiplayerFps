#pragma once
#include "Framework/Hashset.h"
#include "Framework/Hashmap.h"
#include <span>
#include <unordered_map>
class Texture;


struct StreamTextureData;
using std::unordered_map;
// determines what per lod factor per texture
class StreamingTextureMgr {
public:
	static StreamingTextureMgr* inst;
	StreamingTextureMgr();
	~StreamingTextureMgr();
	void tick();
	void set_material_screen_size(std::span<float> sizes);
	void register_streaming_texture(Texture* tex);
	void unregister_streaming_texture(Texture* tex);
private:
	unordered_map<Texture*,StreamTextureData> streaming_textures;
};
