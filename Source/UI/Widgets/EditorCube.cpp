#include "EditorCube.h"
#include "UI/UIBuilder.h"
#include "Framework/MeshBuilder.h"
#include "Render/RenderWindow.h"
#include "Debug.h"
#include "Assets/AssetDatabase.h"
#include "Render/Texture.h"
#include "UI/GUISystemPublic.h"
#include "Framework/Config.h"
ConfigVar editorcube("editorcube", "0", CVAR_BOOL, "");
guiEditorCube::guiEditorCube()
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
void guiEditorCube::draw(RenderWindow& builder)
{
	auto& mb = builder.meshbuilder;
	auto transform_to_screen = [&](glm::vec3 in_clip) {
		return glm::vec3((in_clip.x+1)*ws_sz.x*0.5 + ws_position.x, (in_clip.y + 1) * ws_sz.y*0.5 + ws_position.y, 0.0);
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
	glm::mat4 proj = glm::perspective(glm::radians(90.f), 1.f, 0.001f, 100.f);
	for (int i = 0; i < 36; i++) {
		corners[i].z *= -1;
		corners[i] =  (rotation_matrix) * (corners[i]);
		corners[i].z -= 7.f;
		glm::vec4 v = proj * glm::vec4(corners[i], 1.f);
		v /= v.w;
		corners[i] = v;
		corners[i].y *= -1;
		corners[i] *= 7.0;
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
		mb.AddTriangle(start+2, start , start + 1);
		mb.AddTriangle(start+5, start + 3, start + 4);
		builder.add_draw_call(UiSystem::inst->get_default_ui_mat(), starti, textures.at(i));
	}
}