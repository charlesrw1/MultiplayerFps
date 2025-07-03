#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <vector>
#include "Framework/ClassBase.h"
#include "Framework/StringUtil.h"	// string view
#include "Framework/Util.h"
#include "Framework/Rect2d.h"
#include "Game/EntityComponent.h"
#include "Framework/InlineVec.h"
#include "Framework/EnumDefReflection.h"
template<typename ...Args>
class MulticastDelegate;


struct SDL_KeyboardEvent;
struct SDL_MouseWheelEvent;
class GuiSystemLocal;
class UIBuilder;
/// <summary>
/// 
/// </summary>
/// <param name=""></param>
/// <param name=""></param>
NEWENUM(guiAlignment,uint8_t)
{
	Left, 
	Center, 
	Right, 
	Fill
};
NEWENUM(guiAnchor, uint8_t)
{
	TopLeft,	// (0,0)
	TopRight,	// (1,0)
	BotLeft,	// (0,1)
	BotRight,	// (1,1)
	Center,	//(0.5,0.5)
	Top,	// (0.5,0)
	Bottom,	// (0.5,1)
	Right,	//(0,0.5)
	Left,	// (1,0.5)
};


struct UIAnchorPos
{
	UIAnchorPos() {
		memset(positions, 0, sizeof(positions));
	}
	uint8_t positions[2][2];

	glm::ivec2 to_screen_coord(glm::ivec2 sz, int i) const {
		assert(i >= 0 && i < 2);
		return { sz.x*(positions[0][i] / 255.f),sz.y*(positions[1][i] / 255.f) };
	}
	glm::ivec2 convert_ws_coord(int i, glm::ivec2 ls, glm::ivec2 viewport_pos, glm::ivec2 viewport_sz) const {
		auto anchor = to_screen_coord(viewport_sz, i);
		auto ofs = ls;
		return ofs + anchor + viewport_pos;
	}

	static uint8_t float_to_uint(float f) {
		int i = f * 255.f;
		return (uint8_t)glm::clamp(i, 0, 255);
	}
	static UIAnchorPos create_single(float x, float y) {
		UIAnchorPos ui;
		for (int i = 0; i < 2; i++) {
			ui.positions[0][i] = float_to_uint(x);
			ui.positions[1][i] = float_to_uint(y);
		}
		return ui;
	}
	static UIAnchorPos anchor_from_enum(guiAnchor a);
	static glm::vec2 get_anchor_vec(guiAnchor e);

};
