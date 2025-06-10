#pragma once
#include "UI/BaseGUI.h"
#include <vector>
class Texture;
class guiEditorCube : public guiBase
{
public:
	CLASS_BODY(guiEditorCube);
	guiEditorCube() {
		set_call_init_in_editor(true);
	}

	void paint(UIBuilder& builder) final;
	void update_widget_size() final;
	void start() final;

	std::vector<const Texture*> textures;
	glm::mat3 rotation_matrix = glm::mat3(1.f);
	bool is_ortho = false;
};