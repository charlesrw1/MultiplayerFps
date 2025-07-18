#include "GUISystemPublic.h"
#include "UIBuilder.h"
void Canvas::draw_text(std::string str, int x, int y)
{
	TextShape t;
	t.text = str;
	t.rect.x = x;
	t.rect.y = y;
	UiSystem::inst->window.draw(t);
}

lRect Canvas::calc_text_size(std::string str)
{
	auto r = GuiHelpers::calc_text_size(str, nullptr);
	return lRect(r);
}

void Canvas::draw_rect(lRect rect, Texture* t, lColor color)
{
	RectangleShape shape;
	shape.rect = rect.to_rect2d();
	shape.color = color.to_color32();
	shape.texture = t;
	UiSystem::inst->window.draw(shape);
}

lRect Canvas::get_window_rect()
{
	return lRect(UiSystem::inst->get_vp_rect());
}
#include <SDL2/SDL.h>
#include "GameEnginePublic.h"
void Canvas::set_window_fullscreen(bool is_fullscreen)
{
	SDL_SetWindowFullscreen(eng->get_os_window(), (is_fullscreen) ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void Canvas::set_window_title(std::string name)
{
	SDL_SetWindowTitle(eng->get_os_window(), name.c_str());
}

void Canvas::set_window_capture_mouse(bool capturing_mouse)
{
	UiSystem::inst->set_game_capture_mouse(capturing_mouse);
}
