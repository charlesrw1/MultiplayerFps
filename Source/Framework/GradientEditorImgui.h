#pragma once
#include "imgui.h"
#include "Game/Particles/ParticleTypes.h"

class GradientEditorImgui
{
public:
	// non-interactive preview bar; returns true if clicked
	bool draw_preview(const char* label, const Gradient& gradient, float width = 0.f, float height = 20.f);

	// full interactive editor (for use inside a window)
	bool draw_editor(const char* label, Gradient& gradient, float width = 0.f, float height = 30.f);

	void draw_popup(const char* popup_id, Gradient& gradient);

	// legacy inline editor (preview + interaction combined)
	bool draw_inline(const char* label, Gradient& gradient, float width = 0.f, float height = 20.f);

private:
	void draw_gradient_bar(const Gradient& gradient, ImVec2 pos, ImVec2 size);

	int selected_key = -1;
	bool popup_open = false;
	float color_edit[4] = {1, 1, 1, 1};
};
