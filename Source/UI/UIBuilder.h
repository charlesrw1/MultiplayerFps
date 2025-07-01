#pragma once
#include <string_view>
#include "Framework/Util.h"
#include "Framework/Rect2d.h"
#include <glm/glm.hpp>
#include "BaseGUI.h"
class MaterialInstance;
class GuiFont;
class MeshBuilder;
class GuiSystemLocal;
struct UIBuilderImpl;
class Texture;
class UiSystem;

class RenderWindow;

class GuiHelpers
{
public:
	static Rect2d calc_text_size(std::string_view sv, const GuiFont* font, int force_width = -1);
	static Rect2d calc_text_size_no_wrap(std::string_view sv, const GuiFont* font);

	// in_pos = what you are trying to layout
	// anchor = what position on screen. like top left, center, etc.
	static glm::ivec2 calc_layout(glm::ivec2 in_pos, guiAnchor anchor, Rect2d viewport);
};