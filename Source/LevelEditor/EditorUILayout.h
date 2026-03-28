#pragma once
#include "AllHeader.h"
#include "UI/GUISystemPublic.h"
#include "UI/Widgets/EditorCube.h"
class guiEditorCube;
class guiText;
class EditorUILayout
{
public:
	EditorUILayout(EditorDoc& doc);

	bool draw(EditorInputs& inputs);
	void do_box_select(MouseSelectionAction action);
	Rect2d convert_rect(Rect2d screenSpaceRect) {
		Rect2d out = screenSpaceRect;
		auto pos = UiSystem::inst->get_vp_rect().get_pos();
		out.x -= pos.x;
		out.y -= pos.y;
		return out;
	}
	struct obj
	{
		glm::vec3 pos = glm::vec3(0.f);
		const Entity* e = nullptr;
	};
	std::vector<EditorUILayout::obj> get_objs();

	guiEditorCube cube;
	EditorDoc* doc = nullptr;
};
