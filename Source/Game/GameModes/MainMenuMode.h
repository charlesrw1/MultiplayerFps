#pragma once
#include "Game/GameMode.h"

#include "UI/Widgets/Layouts.h"
#include "UI/Widgets/Interactables.h"

#include "UI/Widgets/Visuals.h"
#include "UI/GUISystemPublic.h"

#include "GameEnginePublic.h"

class MainMenuUILayout : public GUIFullscreen
{
public:
	static GUIButton* create_button(const char* textstr) {
		GUIButton* button = new GUIButton;
		button->padding = { 5,5,5,5 };
		button->w_alignment = GuiAlignment::Fill;

		GUIText* text = new GUIText;
		text->text = textstr;
		text->color = COLOR_WHITE;
		text->ls_position = { 0,0 };
		text->w_alignment = GuiAlignment::Left;
		text->h_alignment = GuiAlignment::Center;
		text->padding = { 10,10,10,10 };


		button->add_this(text);

		return button;
	}

	void start_game() {
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, "map thisIsTheMap.txt\n");
	}

	void exit_game() {
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, "quit\n");
	}
	void open_editor() {
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, "start_ed Map mainMenuMap.txt");
	}

	MainMenuUILayout() {
		auto vbox = new GUIVerticalBox;
		vbox->ls_position = { 100,100 };
		vbox->ls_sz = { 200,500 };

		GUIButton* b = create_button("PLAY");
		b->on_selected.add(this, &MainMenuUILayout::start_game);
		vbox->add_this(b);

		b = create_button("EXIT GAME");
		b->on_selected.add(this, &MainMenuUILayout::exit_game);
		vbox->add_this(b);

		b = create_button("OPEN EDITOR");
		b->on_selected.add(this, &MainMenuUILayout::open_editor);
		vbox->add_this(b);

		add_this(vbox);
	}

};
CLASS_H(MainMenuMode, GameMode)
public:

	void init() {
		ui = new MainMenuUILayout;
		eng->get_gui()->add_gui_panel_to_root(ui);
	}

	void start() {

	}

	void tick() {

	}

	void end() {
		ui->unlink_and_release_from_parent();
		delete ui;
		ui = nullptr;
	}

	void on_player_create(int slot, Player* p) {

	}

	MainMenuUILayout* ui = nullptr;
};
