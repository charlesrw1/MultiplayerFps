#pragma once

#include "AllHeader.h"

struct DragDetector : public IInputReciever
{
	DragDetector() {}
	string get_name() final { return "drag detector"; }
	void on_focused_tick(EditorInputs& inputs) final;
	void tick(EditorInputs& inputs, bool can_start_drag);
	bool get_is_dragging() const;
	Rect2d get_drag_rect() const;

	viewMulticastDelegate<Rect2d> on_drag_end() { return on_drag_end_internal; };
private:
	MulticastDelegate<Rect2d> on_drag_end_internal;
	void end_drag_func(EditorInputs& inputs);

	bool is_dragging = false;
	int mouseClickX = 0;
	int mouseClickY = 0;
};