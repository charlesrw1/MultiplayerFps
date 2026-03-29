#pragma once
#include "AllHeader.h"
#include "UI/GUISystemPublic.h"
#include "UI/Widgets/EditorCube.h"
#include <functional>
class guiEditorCube;
class guiText;
class IEditorApi2;
class EditorUILayout
{
public:
	EditorUILayout(IEditorApi2& doc);

	bool draw(EditorInputs& inputs, std::function<void()> draw_window);
	void do_box_select(MouseSelectionAction action, Rect2d area);
	Rect2d convert_rect(Rect2d screenSpaceRect) const {
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
	IEditorApi2& doc;
};
