#pragma once
#include "LEPlugin.h"
#include "EditorDocLocal.h"
#include "Game/Components/TileMapComponent.h"
NEWCLASS(LETileMapPlugin,LEPlugin)
public:
	bool can_start() final {
		tilemapEntity = ed_doc.selection_state->get_only_one_selected();
		if (!tilemapEntity || !tilemapEntity->get_component<TileMapComponent>()) {
			eng->log_to_fullscreen_gui(Error, "Need to select a tilemap before starting tilemap plugin");
			return false;
		}
		return true;
	}
	void on_start() final {

	}
	void on_end() final {

	}
	void on_update() final {

	}
	void imgui_draw() final {

	}
	EntityPtr tilemapEntity;
};