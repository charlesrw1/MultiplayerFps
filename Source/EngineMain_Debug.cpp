#define IMGUI_DEFINE_MATH_OPERATORS
#include <SDL3/SDL.h>
#include "glad/glad.h"
#include <cstdio>
#include <vector>
#include <string>
#include "GameEngineLocal.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "Framework/MeshBuilder.h"
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "Render/ModelManager.h"
#include "Framework/SysPrint.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "UI/GUISystemPublic.h"
#include "DebugConsole.h"
#include "EditorPopups.h"
#include "Framework/Util.h"
#include "Input/InputSystem.h"
#include <mutex>

#include "Logging.h"

struct Debug_Shape
{
	enum type
	{
		sphere,
		line,
		box,
		transformed_box
	} type;
	glm::vec3 pos;
	glm::vec3 size;
	glm::mat4 transform;
	Color32 color;
	float lifetime = 0.0;
};
class DebugShapeCtx
{
public:
	static DebugShapeCtx& get() {
		static DebugShapeCtx inst;
		return inst;
	}

	void init() {
		MeshBuilder_Object obj;
		obj.visible = false;
		obj.depth_tested = false;
		obj.use_background_color = true;
		this->handle = idraw->get_scene()->register_meshbuilder();
		idraw->get_scene()->update_meshbuilder(handle, obj);
	}
	void update(float dt);
	void add(Debug_Shape shape, bool fixedupdate) {
		if (shape.lifetime <= 0.f && fixedupdate)
			one_frame_fixedupdate.push_back(shape);
		else
			shapes.push_back(shape);
	}
	void fixed_update_start();

private:
	std::vector<Debug_Shape> shapes;
	std::vector<Debug_Shape> one_frame_fixedupdate;
	handle<MeshBuilder_Object> handle;
	MeshBuilder mb;
};

// Free functions used by EngineMain_Loop.cpp and other translation units
void debug_shape_ctx_update(float dt) {
	DebugShapeCtx::get().update(dt);
}
void debug_shape_ctx_fixed_update_start() {
	DebugShapeCtx::get().fixed_update_start();
}

void Debug::add_line(glm::vec3 f, glm::vec3 to, Color32 color, float duration, bool fixedupdate) {
	Debug_Shape shape;
	shape.type = Debug_Shape::line;
	shape.pos = f;
	shape.size = to;
	shape.color = color;
	shape.lifetime = duration;
	DebugShapeCtx::get().add(shape, fixedupdate);
}
void Debug::add_box(glm::vec3 c, glm::vec3 size, Color32 color, float duration, bool fixedupdate) {
	Debug_Shape shape;
	shape.type = Debug_Shape::box;
	shape.pos = c;
	shape.size = size;
	shape.color = color;
	shape.lifetime = duration;
	DebugShapeCtx::get().add(shape, fixedupdate);
}
void Debug::add_transformed_box(glm::mat4 c, glm::vec3 size, Color32 color, float duration, bool fixedupdate) {
	Debug_Shape shape;
	shape.type = Debug_Shape::transformed_box;
	shape.transform = c;
	shape.size = size;
	shape.color = color;
	shape.lifetime = duration;
	DebugShapeCtx::get().add(shape, fixedupdate);
}
void Debug::add_sphere(glm::vec3 c, float radius, Color32 color, float duration, bool fixedupdate) {
	Debug_Shape shape;
	shape.type = Debug_Shape::sphere;
	shape.pos = c;
	shape.size = vec3(radius);
	shape.color = color;
	shape.lifetime = duration;
	DebugShapeCtx::get().add(shape, fixedupdate);
}

void DebugShapeCtx::update(float dt) {

	auto& builder = mb;
	builder.Begin();

	vector<Debug_Shape>* shapearrays[2] = {&one_frame_fixedupdate, &shapes};
	for (int i = 0; i < 2; i++) {
		vector<Debug_Shape>& shapes = *shapearrays[i];
		for (int j = 0; j < shapes.size(); j++) {
			switch (shapes[j].type) {
			case Debug_Shape::line:
				builder.PushLine(shapes[j].pos, shapes[j].size, shapes[j].color);
				break;
			case Debug_Shape::box:
				builder.PushLineBox(shapes[j].pos - shapes[j].size * 0.5f, shapes[j].pos + shapes[j].size * 0.5f,
									shapes[j].color);
				break;
			case Debug_Shape::sphere:
				builder.AddSphere(shapes[j].pos, shapes[j].size.x, 8, 6, shapes[j].color);
				break;
			case Debug_Shape::transformed_box:
				builder.PushOrientedLineBox(glm::vec3(0.f), shapes[j].size, shapes[j].transform, shapes[j].color);
				break;
			}
		}
	}
	builder.End();
	MeshBuilder_Object mbo;
	mbo.transform = glm::mat4(1.f);
	mbo.owner = nullptr;
	mbo.meshbuilder = &mb;
	mbo.visible = true;
	mbo.depth_tested = false;
	mbo.use_background_color = true;

	idraw->get_scene()->update_meshbuilder(handle, mbo);

	for (int i = 0; i < shapes.size(); i++) {
		shapes[i].lifetime -= dt;
		if (shapes[i].lifetime <= 0.f) {
			shapes.erase(shapes.begin() + i);
			i--;
		}
	}
}
void DebugShapeCtx::fixed_update_start() {
	one_frame_fixedupdate.clear();
}

// Called from GameEngineLocal::init
void init_debug_shape_ctx() {
	DebugShapeCtx::get().init();
}

//
// Debug_Interface implementation
//
ConfigVar debug_menu_width("debug.menu_width", "275", CVAR_INTEGER | CVAR_UNBOUNDED, "");
class Debug_Interface_Impl : public Debug_Interface
{
public:
	template <typename FUNC> void add_hook_shared(const char* menu_name, FUNC&& f) {
		int i = 0;
		for (; i < hooks.size(); i++) {
			if (strcmp(menu_name, hooks[i].menu_name) == 0) {
				Hook_Node* node = new Hook_Node;
				// node->drawfunc = drawfunc;
				f(node);
				node->next = nullptr;
				hooks[i].tail->next = node;
				hooks[i].tail = node;
				break;
			}
		}
		if (i == hooks.size()) {
			hooks.push_back(Menu_Hook());
			auto& hook = hooks.back();
			hook.menu_name = menu_name;
			Hook_Node* node = new Hook_Node;
			//	node->drawfunc = drawfunc;
			f(node);
			node->next = nullptr;
			hook.tail = hook.head = node;
		}
	}

	void add_hook(const char* menu_name, void (*drawfunc)()) final {
		add_hook_shared(menu_name, [&](Hook_Node* node) { node->drawfunc = drawfunc; });
	}
	void add_hook_w_name(const char* menu_name, void (*drawfunc)(const char*)) final {
		add_hook_shared(menu_name, [&](Hook_Node* node) { node->drawfunc2 = drawfunc; });
	}

	void draw() {
		ImVec2 winsize = ImGui::GetMainViewport()->Size;
		int width = debug_menu_width.get_integer();
		ImGui::SetNextWindowPos(ImVec2(winsize.x - width - 10, 50));
		ImGui::SetNextWindowSize(ImVec2(width, 700));
		ImGui::SetNextWindowBgAlpha(0.3);
		ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar |
								 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
		if (ImGui::Begin("Debug", nullptr, flags)) {
			ImGui::PushItemWidth(140.f);
			for (int i = 0; i < hooks.size(); i++) {
				if (ImGui::CollapsingHeader(hooks[i].menu_name)) {
					Hook_Node* p = hooks[i].head;
					while (p) {
						p->do_call(hooks[i].menu_name);
						p = p->next;
					}
				}
			}
		}
		ImGui::End();
	}

	struct Hook_Node
	{
		Hook_Node* next = nullptr;

		void do_call(const char* menu_name) {
			if (drawfunc)
				drawfunc();
			else if (drawfunc2)
				drawfunc2(menu_name);
		}
		void (*drawfunc2)(const char*) = nullptr;
		void (*drawfunc)() = nullptr;
	};

	struct Menu_Hook
	{
		const char* menu_name = "";
		Hook_Node* head = nullptr;
		Hook_Node* tail = nullptr;
	};
	std::vector<Menu_Hook> hooks;
};

Debug_Interface* Debug_Interface::get() {
	static Debug_Interface_Impl inst;
	return &inst;
}

//
// Debug_Console implementation
//

int debug_console_text_callback(ImGuiInputTextCallbackData* data) {
	Debug_Console* console = (Debug_Console*)data->UserData;
	if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
		if (data->EventKey == ImGuiKey_UpArrow) {
			if (console->history_index == -1) {
				console->history_index = console->history.size() - 1;
			} else {
				console->history_index--;
				if (console->history_index < 0)
					console->history_index = 0;
			}
		} else if (data->EventKey == ImGuiKey_DownArrow) {
			if (console->history_index != -1) {
				console->history_index++;
				if (console->history_index >= console->history.size())
					console->history_index = console->history.size() - 1;
			}
		}
		console->scroll_to_bottom = true;
		if (console->history_index != -1) {
			auto& hist = console->history[console->history_index];
			data->DeleteChars(0, data->BufTextLen);
			data->InsertChars(0, hist.c_str());
		}
	} else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
		string match = Cmd_Manager::get()->print_matches(console->input_buffer);
		console->scroll_to_bottom = true;
		if (!match.empty()) {
			data->DeleteChars(0, data->BufTextLen);
			data->InsertChars(0, match.c_str());
		}
	}

	return 0;
}

void Debug_Console::draw() {
	{
		std::lock_guard<std::mutex> printLock(printMutex);
		for (auto& l : bufferedLines)
			lines.push_back(std::move(l));
		bufferedLines.clear();
	}
	if (lines.size() > 1000)
		lines.clear();

	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Console")) {
		ImGui::End();

		return;
	}

	ImGui::PushStyleColor(ImGuiCol_ChildBg, uint32_t(ImColor(5, 5, 5)));

	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false)) {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
		for (int i = 0; i < lines.size(); i++) {
			Color32 color;

			auto& line = lines[i];
			auto& lineStr = line.line;
			auto lineColor = line.color;

			// if (!lines[i].empty() && lines[i][0] == '>') {
			//	color = { 136,23,152 };
			//	has_color = true;
			//}

			ImGui::PushStyleColor(ImGuiCol_Text, lineColor.to_uint());
			ImGui::TextUnformatted(lineStr.c_str());
			ImGui::PopStyleColor();
		}
		if (scroll_to_bottom || (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
			ImGui::SetScrollHereY(1.0f);
		scroll_to_bottom = false;

		ImGui::PopStyleVar();
	}
	ImGui::EndChild();

	ImGui::PopStyleColor();

	ImGui::Separator();

	// Command-line
	bool reclaim_focus = false;
	ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll |
										   ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion;

	if (ImGui::InputText("##Input", input_buffer, IM_ARRAYSIZE(input_buffer), input_text_flags,
						 debug_console_text_callback, this)) {
		char* s = input_buffer;
		if (s[0]) {
			// this will print it to the console
			Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, input_buffer);

			history.push_back(input_buffer);
			scroll_to_bottom = true;

			history_index = -1;
		}
		s[0] = 0;
		reclaim_focus = true;
	}
	is_console_focused = ImGui::IsItemFocused();

	ImGui::SetItemDefaultFocus();

	if (reclaim_focus || wants_toggle_set_focus) {
		ImGui::SetKeyboardFocusHere(-1);
		wants_toggle_set_focus = false;
	}

	if (ImGui::IsItemFocused() && input_buffer[0] != 0) {
		auto matches = Cmd_Manager::inst->get_matches(input_buffer);
		if (!matches.empty()) {
			ImVec2 pos = ImGui::GetItemRectMin();
			ImDrawList* draw_list = ImGui::GetForegroundDrawList();
			const int height = std::min((int)matches.size(), 12);
			ImVec2 font_pos = pos + ImVec2(5, -5 - 12);
			const float frame_padding = 8;
			auto min = ImVec2(pos) - ImVec2(-5 + frame_padding, height * 12 + frame_padding);
			auto max = ImVec2(pos) + ImVec2(500, -3);
			draw_list->AddRectFilled(min - ImVec2(2, 2), max + ImVec2(2, 2), Color32(200, 200, 200, 80).to_uint(), 0.f);
			draw_list->AddRectFilled(min, max, Color32(0, 0, 0, 200).to_uint(), 0.f);
			for (int i = 0; i < height; i++) {
				auto color = COLOR_WHITE.to_uint();
				if (matches[i].is_cmd)
					color = Color32(255, 255, 200).to_uint();

				draw_list->AddText(font_pos, color, matches.at(i).name.c_str());
				font_pos = font_pos - ImVec2(0, 12);
			}
		}
	}
	ImGui::End();
}
void Debug_Console::print_args(Color32 color, const char* fmt, va_list args) {
	std::lock_guard<std::mutex> printLock(printMutex);

	char buf[1024];
	vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
	buf[IM_ARRAYSIZE(buf) - 1] = 0;
	LineAndColor lc;
	lc.color = color;
	lc.line = buf;
	bufferedLines.push_back(lc);
}

void Debug_Console::print(Color32 color, const char* fmt, ...) {
	std::lock_guard<std::mutex> printLock(printMutex);

	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
	buf[IM_ARRAYSIZE(buf) - 1] = 0;
	va_end(args);
	LineAndColor lc;
	lc.color = color;
	lc.line = buf;
	bufferedLines.push_back(lc);
}
