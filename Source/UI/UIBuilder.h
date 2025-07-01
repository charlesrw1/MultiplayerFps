#pragma once
#include <string_view>
#include "Framework/Util.h"
#include "Framework/Rect2d.h"
#include <glm/glm.hpp>

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
	static Rect2d calc_text_size(const char* str, const GuiFont* font, int force_width = -1);
	static Rect2d calc_text_size_no_wrap(const char* str, const GuiFont* font);
};

