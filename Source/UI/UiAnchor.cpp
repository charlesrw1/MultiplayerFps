#include "UiAnchor.h"

glm::vec2 UIAnchorPos::get_anchor_vec(guiAnchor e) {
	using namespace glm;
	auto get_vec = [&]() -> glm::vec2 {
		switch (e) {
		case guiAnchor::TopLeft:
			return vec2(0, 0);
		case guiAnchor::TopRight:
			return vec2(1, 0);
		case guiAnchor::BotLeft:
			return vec2(0, 1);
		case guiAnchor::BotRight:
			return vec2(1, 1);

		case guiAnchor::Top:
			return vec2(0.5, 0);
		case guiAnchor::Bottom:
			return vec2(0.5, 1);
		case guiAnchor::Left:
			return vec2(0, 0.5);
		case guiAnchor::Right:
			return vec2(1, 0.5);

		case guiAnchor::Center:
			return vec2(0.5, 0.5);
		}
		return vec2(0, 0);
	};
	return get_vec();
}
UIAnchorPos UIAnchorPos::anchor_from_enum(guiAnchor e) {
	glm::vec2 v = get_anchor_vec(e);
	return UIAnchorPos::create_single(v.x, v.y);
}

glm::ivec2 calc_layout(glm::ivec2 in_pos, guiAnchor anchor, Rect2d viewport) {
	auto sz = viewport.get_size();
	switch (anchor) {
	case guiAnchor::TopLeft:
		return in_pos;
		break;
	case guiAnchor::TopRight:
		return {sz.x + in_pos.x, in_pos.y};
		break;
	case guiAnchor::BotLeft:
		return {in_pos.x, sz.y + in_pos.y};
		break;
	case guiAnchor::BotRight:
		return {sz.x + in_pos.x, sz.y + in_pos.y};
		break;
	case guiAnchor::Center:
		return {in_pos.x + sz.x / 2, sz.y / 2 + in_pos.y};
		break;
	case guiAnchor::Top:
		return {in_pos.x + sz.x / 2, in_pos.y};
		break;
	case guiAnchor::Bottom:
		return {in_pos.x + sz.x / 2, in_pos.y + sz.y};
		break;
	case guiAnchor::Right:
		return {in_pos.x + sz.x, in_pos.y + sz.y / 2};
		break;
	case guiAnchor::Left:
		return {in_pos.x, in_pos.y + sz.y / 2};
		break;
	default:
		return in_pos;
		break;
	}
}
