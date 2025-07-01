#include "BaseGUI.h"

glm::vec2 UIAnchorPos::get_anchor_vec(guiAnchor e)
{
	using namespace glm;
	auto get_vec = [&]() -> glm::vec2 {
		switch (e) {
		case guiAnchor::TopLeft: return vec2(0, 0);
		case guiAnchor::TopRight: return vec2(1, 0);
		case guiAnchor::BotLeft: return vec2(0, 1);
		case guiAnchor::BotRight: return vec2(1, 1);

		case guiAnchor::Top: return vec2(0.5, 0);
		case guiAnchor::Bottom: return vec2(0.5, 1);
		case guiAnchor::Left: return vec2(0, 0.5);
		case guiAnchor::Right: return vec2(1, 0.5);

		case guiAnchor::Center: return vec2(0.5, 0.5);
		}
		return vec2(0, 0);
	};
	return get_vec();
}
UIAnchorPos UIAnchorPos::anchor_from_enum(guiAnchor e)
{
	glm::vec2 v = get_anchor_vec(e);
	return UIAnchorPos::create_single(v.x, v.y);
}
