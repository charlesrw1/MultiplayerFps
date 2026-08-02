#pragma once
#include "Framework/Config.h"
#include <deque>
#include <string>

#include "Framework/Util.h"
#include "FontAsset.h"
#include "UI/Canvas2d.h"
#include "Assets/AssetDatabase.h"
extern ConfigVar ui_disable_screen_log;
class OnScreenLog
{
public:
	void draw() {
		if (ui_disable_screen_log.get_bool())
			return;

		auto font = g_assets.find<FontAsset>("eng/fonts/monospace12.fnt").get();

		float time_now = GetTime();
		float total_time = time_at_full_opacity + time_to_fade;
		while (!items.empty()) {
			if (time_now - items.front().timestamp > total_time)
				items.pop_front();
			else
				break;
		}
		int height = 0;
		for (auto it = items.rbegin(); it != items.rend(); ++it) {
			const auto& item_text = (*it).text;

			Color32 color = it->color;

			float time = time_now - it->timestamp;

			const float entrance_time = 0.2;

			glm::ivec2 offset = {0, height * font->lineHeight};
			if (time < entrance_time) {
				float x = (entrance_time - time);
				x = x * x;
				color.a = int((1 - x) * 255.f);
				offset.x = x * 150;
			}

			if ((total_time - time) < time_to_fade)
				color.a = int((total_time - time) / time_to_fade * 255.f);

			glm::ivec2 texoffset = {0, font->base};
			offset.x += 10;
			offset.y += 10;

			glm::ivec2 pos = offset + texoffset;
			Canvas2d::draw_text(item_text, pos.x, pos.y, lColor{color.r, color.g, color.b, color.a},
								 (FontAsset*)font, guiAnchor::TopLeft);

			height++;
		}
	}

	struct Item
	{
		double timestamp = 0.0;
		std::string text;
		Color32 color{};
	};

	void add_text(Color32 color, std::string text) {
		Item i;
		i.color = color;
		i.text = text;
		i.timestamp = GetTime();
		items.push_back(i);
	}

	std::deque<Item> items;
	float time_at_full_opacity = 3.0;
	float time_to_fade = 1.0;
};