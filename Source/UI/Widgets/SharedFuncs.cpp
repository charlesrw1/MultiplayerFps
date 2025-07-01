#include "SharedFuncs.h"

#if 0
class LayoutUtils
{
public:
	static int get_coord_for_align(int min, int max, guiAlignment align, int size, int& outsize) {
		outsize = size;
		if (align == guiAlignment::Left)
			return min;
		if (align == guiAlignment::Right)
			return max - size;
		if (align == guiAlignment::Center) {
			auto w = max - min;
			return (w - size) * 0.5 + min;
		}
		// else align == fill
		outsize = (max - min);
		return min;
	}

};

void update_one_child_position(guiBase* me)
{
	InlineVec<guiBase*, 16> children;
	me->get_gui_children(children);


	if (children.size() == 1) {
		auto pad = children[0]->get_padding();
		auto corner = me->ws_position + glm::ivec2(pad.x, pad.y);
		auto sz = me->ws_size - glm::ivec2(pad.x + pad.z, pad.y + pad.w);

		auto get_coord_for_align = [](int min, int max, guiAlignment align, int size, int& outsize)->int {
			outsize = size;
			if (align == guiAlignment::Left)
				return min;
			if (align == guiAlignment::Right)
				return max - size;
			if (align == guiAlignment::Center) {
				auto w = max - min;
				return (w - size) * 0.5 + min;
			}
			// else align == fill
			outsize = (max - min);
			return min;
		};
		glm::ivec2 out_corner{};
		glm::ivec2 out_sz{};

		const auto actual_child_sz = children[0]->get_actual_sz_to_use();

		out_corner.x = get_coord_for_align(corner.x, corner.x + sz.x, children[0]->w_alignment, actual_child_sz.x, out_sz.x);
		out_corner.y = get_coord_for_align(corner.y, corner.y + sz.y, children[0]->h_alignment, actual_child_sz.y, out_sz.y);


		children[0]->ws_position = out_corner;
		children[0]->ws_size = out_sz;
	}
}
void update_desired_size_from_one_child(guiBase* me)
{
	InlineVec<guiBase*, 16> children;
	me->get_gui_children(children);

	if (children.size() == 0)
		me->desired_size = { 50,50 };
	else {
		const auto pad = children[0]->get_padding();
		const auto actual_child_sz = children[0]->get_actual_sz_to_use();
		me->desired_size = { pad.x + pad.z + actual_child_sz.x,
		pad.y + pad.w + actual_child_sz.y };
	}
}
void update_desired_size_flow(guiBase* me, int axis)
{
	assert(axis == 0 || axis == 1);
	const int other_axis = (axis == 1) ? 0 : 1;

	me->desired_size = me->get_ls_size();

	glm::ivec2 cursor = { 0,0 };

	InlineVec<guiBase*, 16> children;
	me->get_gui_children(children);


	for (int i = 0; i < children.size(); i++) {
		auto child = children[i];

		const auto pad = child->get_padding();
		const auto actual_child_sz = child->get_actual_sz_to_use();
		const auto sz = actual_child_sz + glm::ivec2(pad.x + pad.z, pad.y + pad.w);

		cursor[other_axis] = glm::max(cursor[other_axis], sz[other_axis]);
		cursor[axis] += sz[axis];
	}
	me->desired_size = cursor;
}
void update_child_positions_flow(guiBase* me, int axis, int start)
{
	assert(axis == 0 || axis == 1);
	const int other_axis = (axis == 1) ? 0 : 1;

	glm::ivec2 cursor = me->ws_position;

	InlineVec<guiBase*, 16> children;
	me->get_gui_children(children);

	for (int i = 0; i < children.size(); i++) {
		auto child = children[i];

		const auto pad = child->get_padding();
		const auto corner = cursor + glm::ivec2(pad.x, pad.y);
		const auto sz = me->ws_size - glm::ivec2(pad.x + pad.z, pad.y + pad.w);
		const auto actual_child_sz = child->get_actual_sz_to_use();

		glm::ivec2 out_corner = cursor;
		out_corner[axis] += pad[axis];
		glm::ivec2 out_sz = { 0,actual_child_sz.y + pad.y + pad.w };
		if (axis == 0) {
			out_sz = { actual_child_sz.x + pad.x + pad.z,0 };
		}
		const auto align = other_axis == 0 ? child->w_alignment : child->h_alignment;
		out_corner[other_axis] = LayoutUtils::get_coord_for_align(corner[other_axis], corner[other_axis] + sz[other_axis], align, actual_child_sz[other_axis], out_sz[other_axis]);


		child->ws_position = out_corner;
		child->ws_position[axis] -= start;
		child->ws_size = out_sz;

		cursor[axis] = out_corner[axis] + out_sz[axis];
	}
}
#endif