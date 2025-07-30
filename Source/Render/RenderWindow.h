#pragma once
#pragma once
#include <vector>
#include "DrawTypedefs.h"
#include "glm/glm.hpp"
#include "Framework/Rect2d.h"
#include "Framework/Util.h"
#include "Framework/MeshBuilder.h"
#include <string>

class MaterialInstance;
class Texture;

struct UIDrawCall {
	int index_start = 0;
	int index_count = 0;
	MaterialInstance* mat = nullptr;
	const Texture* texOverride = nullptr;
};
struct UISetScissor {
	bool enable;
	Rect2d rect;
};
enum class UiDrawCmdType {
	DrawCall,
	SetScissor,
	ClearScissor,
	SetSceneViewProj,
	SetViewProj,
	SetModelMatrix,
	
	SetPipeline,
	SetTexture,

};
struct UiSetScissorCmd {
	int x;
	int y;
	int w;
	int h;
};
struct UiDrawCallCmd {
	int index_start;
	int index_count;
	int base_vertex;
};
struct UiMatrixCmd {
	glm::mat4 matrix;
};
struct UiPipelineCmd {
	MaterialInstance* mat;
};
struct UiTextureCmd {
	Texture* tex;
	int binding;
};

struct UIDrawCmdUnion {
	UiDrawCmdType type{};
	union {
		UiSetScissorCmd scissorCmd;
		UiDrawCallCmd drawCmd;
		UiMatrixCmd matrixCmd;
		UiPipelineCmd pipelineCmd;
		UiTextureCmd textureCmd;
	};
};

// a window for drawing 2d things. sprites, UI, etc.
class GuiFont;
struct RectangleShape {
	Rect2d rect;
	Color32 color=COLOR_WHITE;
	const MaterialInstance* material = nullptr;
	const Texture* texture = nullptr;
	bool with_outline = false;
	Color32 outline_color;
	int outline_width = 0;
};
struct TextShape {
	Rect2d rect;
	Color32 color;
	const GuiFont* font = nullptr;
	std::string_view text;
	bool with_drop_shadow = false;
	Color32 drop_shadow_color = COLOR_BLACK;
	int drop_shadow_ofs = 2;
};

class RenderWindow {
public:
	RenderWindow();
	~RenderWindow();
	RenderWindow(const RenderWindow& other) = delete;
	RenderWindow& operator=(const RenderWindow& other) = delete;

	void clear() { wants_clear = true; }
	void set_view(int x0, int x1, int y0, int y1);
	void add_scissor_rect(Rect2d rect);
	void remove_scissor();
	void add_draw_call(const MaterialInstance* mat, int index_start, const Texture* tex_override = nullptr);
	void draw(RectangleShape rect_shape);
	void draw(TextShape text_shape);
	const std::vector<UIDrawCmdUnion>& get_draw_cmds() const { return drawCmds; }

	glm::mat4 view_mat = glm::mat4();
	Color32 clear_color = {};
	const Texture* render_to_this = nullptr;
	bool is_main_window = false;
	bool wants_clear = false;
	MeshBuilder meshbuilder;
	void reset_verticies() {
		meshbuilder.Begin();
		drawCmds.clear();
		cur_material = nullptr;
		cur_tex_0 = nullptr;
	}
private:
	MaterialInstance* cur_material = nullptr;
	Texture* cur_tex_0 = nullptr;

	std::vector<UIDrawCmdUnion> drawCmds;
};


class MeshBuilder;
class RenderWindowBackend {
public:
	static RenderWindowBackend* inst;
	virtual handle<RenderWindow> register_window() = 0;
	virtual void update_window(handle<RenderWindow> handle, RenderWindow& data) = 0;
	virtual void remove_window(handle<RenderWindow> handle) = 0;
};