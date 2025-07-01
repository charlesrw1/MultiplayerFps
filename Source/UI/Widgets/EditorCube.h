#pragma once
#include "UI/BaseGUI.h"
#include <vector>
#include <glm/glm.hpp>
class Texture;
class RenderWindow;
class guiEditorCube
{
public:
	guiEditorCube();
	void draw(RenderWindow& window);
	glm::ivec2 ws_position = { 10,10 };
	glm::ivec2 ws_sz = { 30,30 };

	std::vector<const Texture*> textures;
	glm::mat3 rotation_matrix = glm::mat3(1.f);
	bool is_ortho = false;
};

