#pragma once

#include "Assets/IAsset.h"
#include "Framework/MulticastDelegate.h"
#include "Game/SerializePtrHelpers.h"
#include <glm/glm.hpp>

#include "Render/MaterialPublic.h"

#include <unordered_map>

class GuiFont;

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

CLASS_H(GuiFont, IAsset)
public:
	int ptSz = 20;
	const Texture* font_texture{};
	std::unordered_map<uint32_t, GuiFontGlyph> character_to_glyph;
	friend class GuiFontLoader;
	friend class GuiHelpers;
};
class GuiFontLoader : public IAssetLoader
{
public:
	IAsset* load_asset(const std::string& path) {
		return (IAsset*)load_font(path);
	}
	void init();
	const GuiFont* load_font(const std::string& fontname);
	const GuiFont* get_default_font() const { return defaultFont; }
private:
	GuiFont* defaultFont{};
	std::unordered_map<std::string, GuiFont*> fonts;
};
extern GuiFontLoader g_fonts;
#include "GUIPublic.h"

#if 0
class Texture;
CLASS_H(ButtonGui, GUI)
public:
	MulticastDelegate<> on_pressed;
	MulticastDelegate<> on_released;

	AssetPtr<MaterialInstance> bg_unselected_mat;
	Color32 bg_unselected_color{};

	AssetPtr<MaterialInstance> bg_selected_mat;
	Color32 bg_selected_color{};

	AssetPtr<MaterialInstance> bg_hovered_mat;
	Color32 bg_hovered_color{};
	 
	AssetPtr<GuiFont> font{};
	Color32 fg_unselected_color{};
	Color32 fg_selected_color{};
	Color32 fg_hovered_color{};
};


CLASS_H(ImageGui, GUI)
public:
	AssetPtr<MaterialInstance> image;
};

CLASS_H(TextGui, GUI)
public:
	AssetPtr<GuiFont> font{};
	Color32 color{};
	bool drop_shadow = false;
};

CLASS_H(HboxGui, GUI)
public:
	float column_gap{};
};
CLASS_H(VboxGui, GUI)
public:
	float row_gap{};
};
CLASS_H(GridGui, GUI)
public:
	float row_gap{};
	float column_gap{};
};

CLASS_H(ManiMenuGui, GUI)
public:
	ManiMenuGui() {

		select_play = new ButtonGui();

		select_play->on_pressed.add(this, &ManiMenuGui::on_set_play);
	}

	void on_set_play() {}


	// Overriding works like this: 
	// derivided class sets the pointer 
	GUI* play_options = nullptr;
	GUI* render_options = nullptr;

	ButtonGui* select_play = nullptr;
	ButtonGui* select_options = nullptr;
	ButtonGui* exit = nullptr;
};

CLASS_H(PlayerHudGui,GUI)
public:
	TextGui* hud_message = nullptr;



};
#endif

