#pragma once
#include "GUISystemPublic.h"
#include "Widgets/Visuals.h"

#include <deque>
#include <string>

#include "Framework/Util.h"
#include "UILoader.h"
CLASS_H(OnScreenLogGui, GUI)
public:

	virtual void paint(UIBuilder& b) {
		auto font = g_fonts.get_default_font();

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

			StringView sv;
			sv.str_start = (*it).text.c_str();
			sv.str_len = (*it).text.size();

			Color32 color = it->color;

			float time = time_now - it->timestamp;

			if ((total_time-time) < time_to_fade)
				color.a = int((total_time-time) / time_to_fade *255.f);

			glm::ivec2 offset = { 0,height * font->ptSz };
			b.draw_text(ws_position + glm::ivec2{ 2,2 } + offset, ws_size, font, sv, { 0,0,0,color.a });
			b.draw_text(ws_position+offset, ws_size, font, sv, color);
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