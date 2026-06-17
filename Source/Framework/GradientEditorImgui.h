#pragma once
#include "imgui.h"
#include "Game/Particles/ParticleTypes.h"

class GradientEditorImgui
{
public:
	bool draw_inline(const char* label, Gradient& gradient, float width = 0.f, float height = 20.f);
	void draw_popup(const char* popup_id, Gradient& gradient);

private:
	int selected_key = -1;
	bool popup_open = false;
	float color_edit[4] = {1, 1, 1, 1};
};
