#pragma once
#include "UI/GUIPublic.h"

CLASS_H(GUIBox, GUI)
public:
	Color32 color;

	virtual void paint(UIBuilder& b) override {
		b.draw_solid_rect(
			ws_position,
			ws_size,
			color
		);
	}
};