#include "EditorCube.h"
#include "UI/UIBuilder.h"
#include "Framework/MeshBuilder.h"
#include "UI/GUISystemLocal.h"
#include "Debug.h"
#include "Assets/AssetDatabase.h"
#include "Render/Texture.h"
ConfigVar editorcube("editorcube", "0", CVAR_BOOL, "");
void guiEditorCube::start()
{
	const string imgs[6] = {
		"cube_z","cube_nz","cube_nx","cube_x","cube_y","cube_ny"
	};
	textures.resize(6);
	for (int i = 0; i < 6; i++) {
		string path = "eng/editor/" + imgs[i] + ".png";
		textures.at(i) = g_assets.find_global_sync<Texture>(path).get();
	}
}
void guiEditorCube::paint(UIBuilder& builder)
{
	auto& mb = builder.get_meshbuilder();
	auto transform_to_screen = [&](glm::vec3 in_clip) {
		return glm::vec3((in_clip.x+1)*ws_size.x*0.5 + ws_position.x, (in_clip.y + 1) * ws_size.y*0.5 + ws_position.y, 0.0);
	};
	glm::vec3 corners[36] = {
		// Front face (CCW order)
		glm::vec3(-1,-1,-1), glm::vec3(1,-1,-1), glm::vec3(1,1,-1),
		glm::vec3(-1,-1,-1), glm::vec3(1,1,-1), glm::vec3(-1,1,-1),
		// Back face (CCW order)
		glm::vec3(-1,-1,1), glm::vec3(-1,1,1), glm::vec3(1,1,1),
		glm::vec3(-1,-1,1), glm::vec3(1,1,1), glm::vec3(1,-1,1),
		// Left face (CCW order)
		glm::vec3(-1,-1,-1), glm::vec3(-1,1,-1), glm::vec3(-1,1,1),
		glm::vec3(-1,-1,-1), glm::vec3(-1,1,1), glm::vec3(-1,-1,1),
		// Right face (CCW order)
		glm::vec3(1,-1,-1), glm::vec3(1,-1,1), glm::vec3(1,1,1),
		glm::vec3(1,-1,-1), glm::vec3(1,1,1), glm::vec3(1,1,-1),
		// Top face (CCW order)
		glm::vec3(-1,1,-1), glm::vec3(1,1,-1), glm::vec3(1,1,1),
		glm::vec3(-1,1,-1), glm::vec3(1,1,1), glm::vec3(-1,1,1),
		// Bottom face (CCW order)
		glm::vec3(-1,-1,-1), glm::vec3(-1,-1,1), glm::vec3(1,-1,1),
		glm::vec3(-1,-1,-1), glm::vec3(1,-1,1), glm::vec3(1,-1,-1),
	};
	const glm::vec2 uvs[36] = {
		// Front face (CCW order)
		glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f),
		glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f),
		// Back face (CCW order)
		glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(0.0f, 0.0f),
		// Left face (CCW order)
		glm::vec2(1, 0), glm::vec2(1,1), glm::vec2(0,1),
		glm::vec2(1,0), glm::vec2(0,1), glm::vec2(0,0),
		// Right face (CCW order)
		glm::vec2(0, 0), glm::vec2(1,0), glm::vec2(1,1),
		glm::vec2(0,0), glm::vec2(1,1), glm::vec2(0,1),
		// Top face (CCW order)
		glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f),
		glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f),
		// Bottom face (CCW order)
		glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(0.0f, 0.0f),
	};
	for (int i = 0; i < 36; i++) {
		corners[i].z *= -1;
		corners[i] =  (rotation_matrix) * (corners[i]);
		corners[i].y *= -1;
		corners[i] = transform_to_screen(corners[i]);
	}

	for (int i = 0; i < 6; i++) {
		const int start = mb.GetBaseVertex();
		const int starti = mb.get_i().size();
		for (int j = 0; j < 6; j++) {
			MbVertex vert;
			vert.position = corners[i * 6 + j];
			vert.uv = uvs[i * 6 + j];
			mb.AddVertex(vert);
		}
		mb.AddTriangle(start, start + 1, start + 2);
		mb.AddTriangle(start+3, start + 4, start + 5);
		builder.add_drawcall(starti, guiSystemLocal.ui_default, textures.at(i));
	}
}

void guiEditorCube::update_widget_size()
{
	desired_size = { ls_w,ls_h };
}
