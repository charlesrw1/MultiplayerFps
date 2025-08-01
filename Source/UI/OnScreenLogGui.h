#pragma once
#include "GUISystemPublic.h"

#include <deque>
#include <string>

#include "Framework/Util.h"
#include "UILoader.h"
#include "Assets/AssetDatabase.h"
#include "UI/UIBuilder.h"
class OnScreenLog
{
public:
	void draw(RenderWindow& b) {
		auto font = g_assets.find_sync<GuiFont>("eng/fonts/monospace12.fnt").get();

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

			std::string_view sv(item_text.c_str(), item_text.size());


			Color32 color = it->color;

			float time = time_now - it->timestamp;

			const float entrance_time = 0.2;

			glm::ivec2 offset = { 0,height * font->lineHeight };
			if (time < entrance_time) {
				float x = (entrance_time - time);
				x = x * x;
				color.a = int((1-x) * 255.f);
				offset.x = x  * 150;
			}

			if ((total_time-time) < time_to_fade)
				color.a = int((total_time-time) / time_to_fade *255.f);

			glm::ivec2 texoffset = { 0,font->base };
			offset.x += 10;
			offset.y += 10;
			//Drawing::draw_text()

			TextShape text;
			text.rect = Rect2d(offset + texoffset, { 0,0 });
			text.font = font;
			text.text = sv;
			text.color = color;
			b.draw(text);

			//b.draw(ws_position + glm::ivec2{ 1,1 } + offset+ texoffset, ws_size, font, sv, { 0,0,0,color.a });
			//b.draw_text(ws_position+offset+ texoffset, ws_size, font, sv, color);
			height++;
		}
	}

	struct Item {
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