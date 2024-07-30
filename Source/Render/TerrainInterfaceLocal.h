#pragma once
#include "Render/TerrainPublic.h"
#include "DrawTypedefs.h"
#include "DrawLocal.h"
#include "glad/glad.h"
enum class TerrainCommand
{
	DrawBrush,
	InvertLayer,
	ClearLayer,
};

struct TerrainEditorQueuedItem
{
	TerrainCommand cmd{};
	const Texture* layer = nullptr;
	handle<EditorTerrainAsset> handle;
	const MaterialInstance* mat{};
	bool additive=false;
	glm::vec2 position = {};
	glm::vec2 size = {};
	float rotation=0.0;
};

class TerrainInterfaceLocal : public TerrainInterfacePublic
{
public:

	static const int ROWS = 20;
	static const int PATCH_POINTS = 4;	// quads
	const float W = 100.0;
	const float H = 100.0f;


	void init() {
		sys_print("*** terraininterface init\n");


		GLint maxTessLevel{};
		glGetIntegerv(GL_MAX_TESS_GEN_LEVEL, &maxTessLevel);
		sys_print("-GL_MAX_TESS_GEN_LEVEL %d\n", maxTessLevel);

		// create vertex buffer for terrain
		struct TerVer {
			glm::vec3 p{};
			glm::vec2 uv{};
		};

		std::vector<TerVer> verts;
		for (int x = 0; x < ROWS; x++) {
			for (int y = 0; y < ROWS; y++) {
				const float u0 = (x / float(ROWS));
				const float v0 = (y / float(ROWS));
				const float u1 = ((x+1) / float(ROWS));
				const float v1 = ((y+1) / float(ROWS));

				const float x0 = u0 *W - W*0.5;
				const float y0 = v0*H - H*0.5;
				const float x1 = u1*W - W*0.5;
				const float y1 = v1*H - H*0.5;

				TerVer v[4];
				v[0].p = glm::vec3(x0, 0, y0);
				v[1].p = glm::vec3(x1, 0, y0);
				v[2].p = glm::vec3(x1, 0, y1);
				v[3].p = glm::vec3(x0, 0, y1);
				v[0].uv = glm::vec2(u0, v0);
				v[1].uv = glm::vec2(u1, v0);
				v[2].uv = glm::vec2(u1, v1);
				v[3].uv = glm::vec2(u0, v1);
				verts.push_back(v[1]);
				verts.push_back(v[0]);
				verts.push_back(v[2]);
				verts.push_back(v[3]);
			}
		}

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(TerVer), verts.data(), GL_STATIC_DRAW);


		// POSITION
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerVer), (void*)offsetof(TerVer, p));
		glEnableVertexAttribArray(0);
		// UV
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TerVer), (void*)offsetof(TerVer, uv));
		glEnableVertexAttribArray(1);
		
		glBindVertexArray(0);

		glPatchParameteri(GL_PATCH_VERTICES, PATCH_POINTS);

		Shader::compile_vert_frag_tess_single_file(&shad, "./Shaders/terrain/TerrainMaster.txt");
	}

	void handle_editor_commands() {

	}

	void draw_to_gbuffer() {

		glBindFramebuffer(GL_FRAMEBUFFER, draw.fbo.gbuffer);

		shad.use();
		shad.set_mat4("ViewProj", draw.current_frame_main_view.viewproj);
		shad.set_mat4("Model", glm::mat4(1));
		auto tex = g_imgs.find_texture("HEIGHTMAP2.png");
		shad.set_float("WorldScale", W);
		shad.set_float("VerticalScale", 12.0);

		draw.bind_texture(0, tex->gl_id);

		glBindVertexArray(vao);

		glDrawArrays(GL_PATCHES, 0, ROWS * ROWS * PATCH_POINTS);
		glBindVertexArray(0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

	}

	handle<CompiliedTerrainAsset> register_terrain(const CompiliedTerrainAsset* asset) override {
		return { -1 };
	}
	void update_terrain(handle<CompiliedTerrainAsset> asset) override {

	}
	void remove_terrain(handle<CompiliedTerrainAsset>& asset) {

	}

	handle<EditorTerrainAsset> register_editor_terrain(const EditorTerrainAsset* asset)override {
		return { -1 };
	}
	void update_editor_terrain(handle<CompiliedTerrainAsset> asset)override {

	}
	void remove_editor_terrain(handle<EditorTerrainAsset>& asset) override {

	}

	void draw_brush_to_layer(
		handle<EditorTerrainAsset> asset,
		const MaterialInstance* mat,
		bool additive,
		glm::vec2 position,
		glm::vec2 size,
		float rotation,
		const Texture* layer) override {

	}
	void invert_layer(
		handle<EditorTerrainAsset> asset,
		const Texture* layer)override {

	}

	Shader shad{};
	vertexarrayhandle vao{};
	vertexbufferhandle vbo{};

	// for editor
	std::vector<TerrainEditorQueuedItem> command_queue;
};