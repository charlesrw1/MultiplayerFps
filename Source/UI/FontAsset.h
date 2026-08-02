#pragma once

#include <glm/glm.hpp>
#include <string_view>
#include <unordered_map>
#include "Render/DynamicMaterialPtr.h"
#include "Framework/Reflection2.h"
#include "Framework/Rect2d.h"
#include "Assets/AssetDatabase.h"
class Texture;
class MaterialInstance;
struct FontGlyph
{
	uint16_t x{};
	uint16_t y{};
	uint16_t w{};
	uint16_t h{};
	int16_t xofs{};
	int16_t yofs{};
	int16_t advance{};
};
class FontAsset : public IAsset
{
public:
	CLASS_BODY(FontAsset);

	int ptSz = 20;
	int lineHeight = 0;
	int base = 0;
	std::shared_ptr<Texture> font_texture{};

	std::unordered_map<uint32_t, FontGlyph> character_to_glyph;

	void uninstall() override {
		character_to_glyph.clear();
		font_texture.reset();
		ptSz = 20;
		lineHeight = 0;
		base = 0;
	}
	bool load_asset();
	void post_load();

	REF static FontAsset* load(const std::string& name) { return g_assets.find<FontAsset>(name).get(); }

	static Rect2d calc_text_size(std::string_view sv, const FontAsset* font, int force_width = -1);
	static Rect2d calc_text_size_no_wrap(std::string_view sv, const FontAsset* font);
};
