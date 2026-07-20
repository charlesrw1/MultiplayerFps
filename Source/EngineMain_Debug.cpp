#define IMGUI_DEFINE_MATH_OPERATORS
#include <SDL3/SDL.h>
#include "glad/glad.h"
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>
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
#include "UI/UILoader.h"
#include "UI/UIBuilder.h"
#include "DebugConsole.h"
#include "EditorPopups.h"
#include "Framework/Util.h"
#include "Input/InputSystem.h"
#include "Render/DrawLocal.h"
#include "Render/ViewSetup.h"
#include "Render/Texture.h"
#include "Assets/AssetDatabase.h"
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

struct Debug_Text
{
	glm::vec3 pos;
	std::string text;
	Color32 color;
	float lifetime = 0.0;
	bool center_horizontal = false;
	bool anchor_bottom = false;
};
// Projects world-space debug text to screen every frame and draws it through the same
// immediate-mode UI window regular HUD text uses (RenderWindow), so it's automatically
// screen-space (no depth test) and gets flushed by the normal UI sync each frame.
class DebugTextCtx
{
public:
	static DebugTextCtx& get() {
		static DebugTextCtx inst;
		return inst;
	}
	void add(Debug_Text text, bool fixedupdate) {
		if (text.lifetime <= 0.f && fixedupdate)
			one_frame_fixedupdate.push_back(std::move(text));
		else
			texts.push_back(std::move(text));
	}
	void draw_and_tick(float dt);
	void fixed_update_start() { one_frame_fixedupdate.clear(); }

private:
	std::vector<Debug_Text> texts;
	std::vector<Debug_Text> one_frame_fixedupdate;
};

void DebugTextCtx::draw_and_tick(float dt) {
	const View_Setup& vs = draw.get_current_frame_vs();
	const Rect2d vp = UiSystem::inst->get_vp_rect();
	const GuiFont* mono_font = GuiFont::load("eng/fonts/monospace12.fnt");

	// Distance (meters) over which text fades from fully opaque to invisible.
	const float fade_start = 4.f;
	const float fade_end   = 30.f;

	auto draw_one = [&](const Debug_Text& t) {
		glm::vec4 clip = vs.viewproj * glm::vec4(t.pos, 1.f);
		if (clip.w < 0.001f)
			return; // behind camera
		glm::vec3 ndc = glm::vec3(clip) / clip.w;
		if (ndc.x < -1.f || ndc.x > 1.f || ndc.y < -1.f || ndc.y > 1.f)
			return; // offscreen

		const float dist = glm::length(t.pos - vs.origin);
		const float fade = 1.f - glm::clamp((dist - fade_start) / (fade_end - fade_start), 0.f, 1.f);
		if (fade <= 0.01f)
			return;

		TextShape shape;
		shape.font  = mono_font;
		shape.color = t.color;
		shape.color.a = (uint8_t)(shape.color.a * fade);
		// RenderWindow's ortho projection (UiSystem::update(), GUISystemLocal.cpp) is built
		// from get_vp_rect().get_size() only, no offset -- window.draw() rects are
		// viewport-local (0,0 = viewport top-left), NOT window-local. Do not add vp.x/vp.y.
		const int16_t x = (int16_t)((ndc.x * 0.5f + 0.5f) * vp.w);
		int16_t y = (int16_t)((1.f - (ndc.y * 0.5f + 0.5f)) * vp.h);
		const int line_height = mono_font ? mono_font->lineHeight : 12;

		if (t.anchor_bottom) {
			const int num_lines = 1 + (int)std::count(t.text.begin(), t.text.end(), '\n');
			y -= (int16_t)(line_height * (num_lines - 1));
		}

		// draw_text_to_meshbuilder treats '\n' as an unknown glyph rather than a line
		// break, so split multi-line text into one TextShape per line here.
		size_t start = 0;
		while (start <= t.text.size()) {
			size_t nl = t.text.find('\n', start);
			size_t end = (nl == std::string::npos) ? t.text.size() : nl;
			shape.text = std::string_view(t.text).substr(start, end - start);
			shape.rect.x = x;
			if (t.center_horizontal)
				shape.rect.x -= (int16_t)(GuiHelpers::calc_text_size_no_wrap(shape.text, mono_font).w / 2);
			shape.rect.y = y;
			UiSystem::inst->window.draw(shape);
			y += (int16_t)line_height;
			if (nl == std::string::npos)
				break;
			start = nl + 1;
		}
	};
	for (auto& t : texts) draw_one(t);
	for (auto& t : one_frame_fixedupdate) draw_one(t);

	for (int i = 0; i < (int)texts.size(); i++) {
		texts[i].lifetime -= dt;
		if (texts[i].lifetime <= 0.f) {
			texts.erase(texts.begin() + i);
			i--;
		}
	}
}

// Free functions used by EngineMain_Loop.cpp and other translation units
void debug_shape_ctx_update(float dt) {
	DebugShapeCtx::get().update(dt);
	DebugTextCtx::get().draw_and_tick(dt);
}
void debug_shape_ctx_fixed_update_start() {
	DebugShapeCtx::get().fixed_update_start();
	DebugTextCtx::get().fixed_update_start();
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
void Debug::add_text(glm::vec3 pos, std::string text, Color32 color, float duration, bool fixedupdate) {
	Debug_Text t;
	t.pos = pos;
	t.text = std::move(text);
	t.color = color;
	t.lifetime = duration;
	DebugTextCtx::get().add(std::move(t), fixedupdate);
}
void Debug::add_text_ex(glm::vec3 pos, std::string text, Color32 color, float duration,
						bool center_horizontal, bool anchor_bottom, bool fixedupdate) {
	Debug_Text t;
	t.pos = pos;
	t.text = std::move(text);
	t.color = color;
	t.lifetime = duration;
	t.center_horizontal = center_horizontal;
	t.anchor_bottom = anchor_bottom;
	DebugTextCtx::get().add(std::move(t), fixedupdate);
}

void Debug::add_circle(glm::vec3 center, glm::vec3 normal, float radius, Color32 color,
					   float lifetime, bool fixedupdate, int segments)
{
	normal = glm::normalize(normal);
	glm::vec3 right;
	if (glm::abs(glm::dot(normal, glm::vec3(0, 1, 0))) < 0.99f)
		right = glm::normalize(glm::cross(normal, glm::vec3(0, 1, 0)));
	else
		right = glm::normalize(glm::cross(normal, glm::vec3(0, 0, 1)));
	glm::vec3 forward = glm::cross(right, normal);

	for (int i = 0; i < segments; i++) {
		float t0 = glm::two_pi<float>() * i / segments;
		float t1 = glm::two_pi<float>() * (i + 1) / segments;
		glm::vec3 p0 = center + (right * glm::cos(t0) + forward * glm::sin(t0)) * radius;
		glm::vec3 p1 = center + (right * glm::cos(t1) + forward * glm::sin(t1)) * radius;
		add_line(p0, p1, color, lifetime, fixedupdate);
	}
}

void Debug::add_cone(glm::vec3 apex, glm::vec3 direction, float length, float angle_degrees,
					 Color32 color, float lifetime, bool fixedupdate, int segments)
{
	direction = glm::normalize(direction);
	float angle_rad = glm::radians(angle_degrees);
	float end_radius = length * glm::tan(angle_rad);
	glm::vec3 tip = apex + direction * length;

	glm::vec3 right;
	if (glm::abs(glm::dot(direction, glm::vec3(0, 1, 0))) < 0.99f)
		right = glm::normalize(glm::cross(direction, glm::vec3(0, 1, 0)));
	else
		right = glm::normalize(glm::cross(direction, glm::vec3(0, 0, 1)));
	glm::vec3 up = glm::cross(right, direction);

	// circle at end
	add_circle(tip, direction, end_radius, color, lifetime, fixedupdate, segments);

	// lines from apex to circle edge
	for (int i = 0; i < 4; i++) {
		float theta = glm::two_pi<float>() * i / 4.f;
		glm::vec3 edge = tip + (right * glm::cos(theta) + up * glm::sin(theta)) * end_radius;
		add_line(apex, edge, color, lifetime, fixedupdate);
	}
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
	mbo.depth_tested = true;
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

	static Texture* filter_icon = g_assets.find<Texture>("eng/icons/filter.png").get();
	static const char* level_names[5] = { "Error", "Warning", "Info", "Debug", "Command" };
	const float FILTER_ICON_SIZE = 16.0f;

	ImGui::PushStyleColor(ImGuiCol_ChildBg, uint32_t(ImColor(5, 5, 5)));

	auto copy_selected_lines = [this]() {
		if (select_anchor_line == -1)
			return;
		int lo = std::min(select_anchor_line, select_end_line);
		int hi = std::max(select_anchor_line, select_end_line);
		std::string clip;
		for (int i = lo; i <= hi && i < (int)lines.size(); i++) {
			if (!show_level[(int)lines[i].type])
				continue;
			clip += lines[i].line;
			clip += "\n";
		}
		ImGui::SetClipboardText(clip.c_str());
	};

	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false)) {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.5f, 0.9f, 0.25f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.9f, 0.45f));
		for (int i = 0; i < lines.size(); i++) {
			auto& line = lines[i];
			if (!show_level[(int)line.type])
				continue;

			// Selectable row (click / shift-click for a range) with the text drawn
			// on top, so lines can be multi-selected and copied without "##" in
			// log text being parsed as an ImGui id.
			bool is_selected = select_anchor_line != -1 &&
								i >= std::min(select_anchor_line, select_end_line) &&
								i <= std::max(select_anchor_line, select_end_line);

			ImGui::PushID(i);
			ImVec2 text_pos = ImGui::GetCursorScreenPos();
			if (ImGui::Selectable("##line", is_selected, ImGuiSelectableFlags_AllowItemOverlap)) {
				if (ImGui::GetIO().KeyShift && select_anchor_line != -1)
					select_end_line = i;
				else {
					select_anchor_line = i;
					select_end_line = i;
				}
			}
			ImGui::SetCursorScreenPos(text_pos);
			ImGui::PushStyleColor(ImGuiCol_Text, line.color.to_uint());
			ImGui::TextUnformatted(line.line.c_str());
			ImGui::PopStyleColor();
			ImGui::PopID();
		}

		if (ImGui::BeginPopupContextWindow("console_copy_menu")) {
			if (ImGui::MenuItem("Copy", "Ctrl+C", false, select_anchor_line != -1))
				copy_selected_lines();
			ImGui::EndPopup();
		}
		if (select_anchor_line != -1 && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
			ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C))
			copy_selected_lines();

		if (scroll_to_bottom || (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
			ImGui::SetScrollHereY(1.0f);
		scroll_to_bottom = false;

		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar();
	}
	ImGui::EndChild();

	ImGui::PopStyleColor();

	ImGui::Separator();

	// Command-line
	bool reclaim_focus = false;
	ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll |
										   ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion;

	if (filter_icon) {
		float input_width = ImGui::GetContentRegionAvail().x - FILTER_ICON_SIZE - ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetNextItemWidth(input_width);
	}
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

	if (filter_icon) {
		ImGui::SameLine();
		ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_Button];
		if (ImGui::ImageButton("##console_filter",
				ImTextureID(uint64_t(filter_icon->get_internal_render_handle())),
				ImVec2(FILTER_ICON_SIZE, FILTER_ICON_SIZE), ImVec2(0, 0), ImVec2(1, 1), bg))
			ImGui::OpenPopup("console_filter_popup");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Filter log levels");
	}
	if (ImGui::BeginPopup("console_filter_popup")) {
		for (int i = 0; i < 5; i++)
			ImGui::Checkbox(level_names[i], &show_level[i]);
		ImGui::EndPopup();
	}

	ImGui::End();
}
void Debug_Console::print_args(LogType type, const char* fmt, va_list args) {
	std::lock_guard<std::mutex> printLock(printMutex);

	char buf[1024];
	vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
	buf[IM_ARRAYSIZE(buf) - 1] = 0;
	LineAndColor lc;
	lc.color = get_color_of_print(type);
	lc.type = type;
	lc.line = buf;
	bufferedLines.push_back(lc);
}

void Debug_Console::print(LogType type, const char* fmt, ...) {
	std::lock_guard<std::mutex> printLock(printMutex);

	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
	buf[IM_ARRAYSIZE(buf) - 1] = 0;
	va_end(args);
	LineAndColor lc;
	lc.color = get_color_of_print(type);
	lc.type = type;
	lc.line = buf;
	bufferedLines.push_back(lc);
}
