#include "DragDetector.h"
#include "EditorDocLocal.h"
void DragDetector::end_drag_func() {
	if (!Input::is_mouse_down(0) && is_dragging) {
		if (get_is_dragging()) {
			printf("end drag\n");
			doc.inputs.set_focus(nullptr);
			on_drag_end.invoke(get_drag_rect());
			doc.inputs.eat_mouse_click();
		}
		is_dragging = false;
		mouseClickX = 0;
		mouseClickY = 0;
	}
}

void DragDetector::on_focused_tick() {
	end_drag_func();
}

void DragDetector::tick(bool can_start_drag) {
	const bool can_start = doc.inputs.can_use_mouse_click();

	if (Input::was_mouse_pressed(0)) {
		if (can_start && !is_dragging && UiSystem::inst->is_vp_hovered()) {
			mouseClickX = Input::get_mouse_pos().x;
			mouseClickY = Input::get_mouse_pos().y;
			is_dragging = true;
			printf("start dragging\n");
		}
	}
	end_drag_func();
	if (get_is_dragging()) {
		printf("start actual dragging\n");
		doc.inputs.set_focus(this);
		doc.inputs.eat_mouse_click();
	}
}

bool DragDetector::get_is_dragging() const {
	if (!is_dragging)
		return false;
	auto rect = get_drag_rect();
	if (rect.w >= 2 || rect.h >= 2) {
		return true;
	}
	return false;
}

Rect2d DragDetector::get_drag_rect() const {
	auto pos = Input::get_mouse_pos();
	glm::ivec2 clickPos = { mouseClickX, mouseClickY };
	Rect2d rect;
	auto minP = glm::min(clickPos, pos);
	auto maxP = glm::max(clickPos, pos);

	rect.x = minP.x;
	rect.y = minP.y;
	rect.w = maxP.x - minP.x;
	rect.h = maxP.y - minP.y;

	return rect;
}
