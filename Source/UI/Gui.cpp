#include "Gui.h"
#include "GUISystemPublic.h"
#include "Render/RenderWindow.h"
#include "UIBuilder.h"

Color32 Gui::current_color = COLOR_WHITE;

void Gui::set_color(float r, float g, float b, float a) {
	current_color = Color32((uint8_t)(glm::clamp(r, 0.f, 1.f) * 255), (uint8_t)(glm::clamp(g, 0.f, 1.f) * 255),
							(uint8_t)(glm::clamp(b, 0.f, 1.f) * 255), (uint8_t)(glm::clamp(a, 0.f, 1.f) * 255));
}

void Gui::rectangle(int x, int y, int w, int h) {
	RectangleShape shape;
	shape.rect = Rect2d(x, y, w, h);
	shape.color = current_color;
	UiSystem::inst->window.draw(shape);
}

void Gui::rectangle_outline(int x, int y, int w, int h, int thickness) {
	LineShape top{glm::ivec2(x, y), glm::ivec2(x + w, y), thickness, current_color};
	LineShape bot{glm::ivec2(x, y + h), glm::ivec2(x + w, y + h), thickness, current_color};
	LineShape lft{glm::ivec2(x, y), glm::ivec2(x, y + h), thickness, current_color};
	LineShape rgt{glm::ivec2(x + w, y), glm::ivec2(x + w, y + h), thickness, current_color};
	UiSystem::inst->window.draw(top);
	UiSystem::inst->window.draw(bot);
	UiSystem::inst->window.draw(lft);
	UiSystem::inst->window.draw(rgt);
}

void Gui::circle(int x, int y, int radius, int segments) {
	CircleShape shape;
	shape.center = glm::ivec2(x, y);
	shape.radius = radius;
	shape.segments = segments;
	shape.filled = true;
	shape.color = current_color;
	UiSystem::inst->window.draw(shape);
}

void Gui::line(int x1, int y1, int x2, int y2, int thickness) {
	LineShape shape{glm::ivec2(x1, y1), glm::ivec2(x2, y2), thickness, current_color};
	UiSystem::inst->window.draw(shape);
}

void Gui::image(Texture* tex, int x, int y, int w, int h) {
	RectangleShape shape;
	shape.rect = Rect2d(x, y, w, h);
	shape.color = current_color;
	shape.texture = tex;
	UiSystem::inst->window.draw(shape);
}

void Gui::print(std::string text, int x, int y) {
	TextShape shape;
	shape.text = text;
	shape.rect.x = x;
	shape.rect.y = y;
	shape.color = current_color;
	UiSystem::inst->window.draw(shape);
}

lRect Gui::measure_text(std::string text) {
	auto r = GuiHelpers::calc_text_size(text, nullptr);
	return lRect(r);
}

lRect Gui::get_screen_size() {
	return lRect(UiSystem::inst->get_vp_rect());
}
