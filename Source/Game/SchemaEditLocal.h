#pragma once
#include "IEditorTool.h"
#include "DrawPublic.h"
#include "Types.h"
#include <string>

#include "Entity.h"
#include "Framework/ArrayReflection.h"
#include "Game_Engine.h"
#include "Framework/ObjectSerialization.h"

class SchemaEditorLocal : public IEditorTool
{
public:
	void draw_menu_bar() override;
	// Inherited via IEditorTool
	virtual void ui_paint() override {}
	virtual bool handle_event(const SDL_Event& event) override {}
	virtual void tick(float dt) override {
		auto window_sz = eng->get_game_viewport_dimensions();
		float aratio = (float)window_sz.y / window_sz.x;
		{
			int x = 0, y = 0;
			if (eng->get_game_focused()) {
				SDL_GetRelativeMouseState(&x, &y);
				camera.update_from_input(eng->keys, x, y, glm::mat4(1.f));
			}
		}

		view = View_Setup(camera.position, camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);
	}
	virtual const View_Setup& get_vs() override {
		return view;
	}
	virtual void overlay_draw() override {}
	virtual void init() override {}
	virtual bool can_save_document() override { return true; }
	virtual const char* get_editor_name() override { return "Schema Editor"; }
	virtual bool has_document_open() const override {
		doc_is_open;
	}

	virtual void on_change_focus(editor_focus_state newstate) override;
	virtual void open_document_internal(const char* name) override {

	}
	virtual void close_internal() override;
	virtual bool save_document_internal() override;
	void imgui_draw() override;

	bool is_open = false;
	View_Setup view;
	User_Camera camera;

	bool doc_is_open = false;

	SchemaFormatFile file;
};