#pragma once
#include <vector>
#include "Util.h"	// color32
void MyImSeperator(float x1, float x2, float width);

bool MyImDrawBlendSpace(
	const char* label,
	const std::vector<ImVec2>& verts, 
	const std::vector<int>& indicies, 
	const std::vector<const char*>& vert_names,
	ImVec2 minbound, 
	ImVec2 maxbound, ImVec2* hover_pos);

inline ImVec4 color32_to_imvec4(Color32 color) {
	return ImVec4(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);
}
inline unsigned int color32_to_int(Color32 color) {
	return *(unsigned int*)&color;
}
namespace ImGui {
	int Curve(const char* label, const ImVec2& size,
		const int maxpoints,
		ImVec2* points,
		int* selection,
		const ImVec2& rangeMin = ImVec2(0, 0),
		const ImVec2& rangeMax = ImVec2(1, 1));
};