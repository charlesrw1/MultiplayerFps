#pragma once
#include "Framework/MapUtil.h"
#include "IInputReciever.h"
#include "glm/glm.hpp"
#include "Game/EntityPtr.h"
using std::string;
enum class VHResult
{
	Unchanged,
	Changing,
	Finished
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
	VHResult box_handles(int64_t id, glm::mat4& box_corner /* no scale*/, glm::vec3& box_extents);
	void tick();
	string get_name() final { return "viewport handles"; }

private:
	EditorDoc& doc;
	struct ActiveItem
	{
		enum type
		{
			NEWLY_MADE,
			POINT,
			BOX
		} mytype;
		// random data
		glm::vec3 pos{};
		glm::mat4 transform{};
		glm::vec3 boxextents{};

		glm::mat4 newtransform{};

		glm::vec3 get_position_for_handle(int idx, bool use_new);
		glm::vec3 get_normal_for_handle(int idx);

		bool was_wanted_this_frame = false;
	};

	std::unordered_map<int64_t, ActiveItem> items;

	struct Dragging
	{
		int64_t item = -1;
		int index = 0; // for boxes
		bool set_next_frame = false;
	};
	bool has_item_being_dragged() const { return dragging_state.item != -1; }

	Dragging dragging_state;
	EntityPtr hacked_entity_MFER; // forgive me lord for i have sinned
	std::vector<EntityPtr> cached_selection_to_return;
};
