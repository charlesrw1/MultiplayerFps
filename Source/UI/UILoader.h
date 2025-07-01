#pragma once


#include "Game/SerializePtrHelpers.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include "Render/DynamicMaterialPtr.h"
#include "Framework/Reflection2.h"
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
class IAssetLoadingInterface;
class GuiFont : public IAsset {
public:
	CLASS_BODY(GuiFont);

	int ptSz = 20;
	int lineHeight = 0;
	int base = 0;
	const Texture* font_texture{};
	std::unordered_map<uint32_t, GuiFontGlyph> character_to_glyph;

	friend class GuiFontLoader;
	friend class GuiHelpers;

	void uninstall() override {
		character_to_glyph.clear();
	}
	bool load_asset(IAssetLoadingInterface*);
	void post_load();
	void move_construct(IAsset* _other) {
		GuiFont* other = (GuiFont*)_other;
		*this = std::move(*other);
	}
	void sweep_references(IAssetLoadingInterface*) const override;
};
