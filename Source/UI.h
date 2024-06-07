#pragma once
#include <vector>
#include <memory>
#include "Framework/Util.h"
#include "Framework/PropertyEd.h"
#include "glm/glm.hpp"
#include "Framework/Rect2d.h"
class UIFont;
class Material;
class Texture;

const int UI_SCREEN_WIDTH = 640;
const int UI_SCREEN_HEIGHT = 480;

enum class UIAnchor : uint8_t
{
	TopL,Top,TopR,
	L,C,R,
	BottomL,Bottom,BottomR
};


// base class for all UI functions
union SDL_Event;
class UIControl
{
public:
	virtual void ui_paint() = 0;
	virtual bool handle_event(const SDL_Event& event) = 0;
	// location of this control
	Rect2d get_size() {
		return size;
	}
	void set_parent(UIControl* p) {
		parent = p;
	}
protected:
	Rect2d size;
	UIControl* parent = nullptr;
};
