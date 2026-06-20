#pragma once
#include "Framework/MapUtil.h"
#include "Framework/MathLib.h"
#include "IInputReciever.h"
#include "glm/glm.hpp"
using std::string;

enum class VHResult
{
	Unchanged,
	Changing,
	Finished
};

enum class BoxHandleMode
{
	Face,
	Edge
};

using glm::mat3;
using glm::mat4;
using glm::normalize;
using glm::quat;
using glm::vec3;
class EditorDoc;
class EViewportHandles : public IInputReciever
{
public:
	EViewportHandles(EditorDoc& doc) : doc(doc) {}
	VHResult point_handle(int64_t id, glm::vec3& inout_position);
	VHResult box_handles(int64_t id, glm::mat4& box_corner, glm::vec3& box_extents,
						 BoxHandleMode mode = BoxHandleMode::Face);
	void tick(EditorInputs& inputs);
	string get_name() final { return "viewport handles"; }

private:
	EditorDoc& doc;
	struct ActiveItem
	{
		bool was_wanted_this_frame = false;
		bool just_finished = false;
		BoxHandleMode mode = BoxHandleMode::Face;

		glm::mat4 transform{};
		glm::vec3 boxextents{};
		glm::mat4 newtransform{};

		glm::vec3 get_position_for_face_handle(int idx, bool use_new) const;
		glm::vec3 get_normal_for_face_handle(int idx) const;

		glm::vec3 get_position_for_edge_handle(int idx, bool use_new) const;
		glm::vec3 get_normal_for_edge_handle(int idx) const;
	};

	std::unordered_map<int64_t, ActiveItem> items;

	struct Dragging
	{
		int64_t item = -1;
		int index = 0;
		glm::vec3 plane_normal{};
		glm::vec3 plane_point{};
		glm::vec3 initial_hit{};
		glm::vec3 initial_handle_pos{};
		glm::mat4 initial_transform{};
		glm::vec3 initial_extents{};
		float display_delta[2] = {};
	};
	bool has_item_being_dragged() const { return dragging_state.item != -1; }

	void tick_face_drag(const glm::vec3& current_hit);
	void tick_edge_drag(const glm::vec3& current_hit);
	void draw_drag_info_text();

	Dragging dragging_state;
};
