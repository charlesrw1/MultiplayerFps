#pragma once

#include "AllHeader.h"

struct DragDetector : public IInputReciever
{
	DragDetector(EditorDoc& doc) {}
	string get_name() final { return "drag detector"; }
	MulticastDelegate<Rect2d> on_drag_end;
	void on_focused_tick(EditorInputs& inputs) final;
	void tick(EditorInputs& inputs, bool can_start_drag);
	bool get_is_dragging() const;
	Rect2d get_drag_rect() const;

private:
	void end_drag_func(EditorInputs& inputs);

	bool is_dragging = false;
	int mouseClickX = 0;
	int mouseClickY = 0;
};