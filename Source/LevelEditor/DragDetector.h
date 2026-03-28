#pragma once

#include "AllHeader.h"

struct DragDetector : public IInputReciever
{
	DragDetector(EditorDoc& doc) : doc(doc) {}
	string get_name() final { return "drag detector"; }
	MulticastDelegate<Rect2d> on_drag_end;
	void on_focused_tick() final;
	void tick(bool can_start_drag);
	bool get_is_dragging() const;
	Rect2d get_drag_rect() const;

private:
	void end_drag_func();
	EditorDoc& doc;
	bool is_dragging = false;
	int mouseClickX = 0;
	int mouseClickY = 0;
};