#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include "Render/DynamicMaterialPtr.h"
#include "Framework/Reflection2.h"
#include "Assets/AssetDatabase.h"
class GuiFont;
class Texture;
class MaterialInstance;
struct GuiFontGlyph
{
	uint16_t x{};
	uint16_t y{};
	uint16_t w{};
	uint16_t h{};
	int16_t xofs{};
	int16_t yofs{};
	int16_t advance{};
};
class GuiFont : public IAsset
{
public:
	CLASS_BODY(GuiFont);

	int ptSz = 20;
	int lineHeight = 0;
	int base = 0;
	std::shared_ptr<Texture> font_texture{};

	std::unordered_map<uint32_t, GuiFontGlyph> character_to_glyph;

	friend class GuiFontLoader;
	friend class GuiHelpers;

	void uninstall() override {
		character_to_glyph.clear();
		font_texture.reset();
		ptSz = 20;
		lineHeight = 0;
		base = 0;
	}
	bool load_asset();
	void post_load();

	REF static GuiFont* load(const std::string& name) { return g_assets.find<GuiFont>(name).get(); }
};
