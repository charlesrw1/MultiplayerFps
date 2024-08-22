#pragma once
#include "Game/GameMode.h"

#include "UI/Widgets/Layouts.h"
#include "UI/Widgets/Interactables.h"

#include "UI/Widgets/Visuals.h"
#include "UI/GUISystemPublic.h"

#include "GameEnginePublic.h"

#include "Sound/SoundPublic.h"

#include "UI/Widgets/Visuals.h"
class GameTransitionUI
{
public:
	GameTransitionUI() {
		loadText = new GUIText;
		loadText->text = "LOADING...";
		loadText->myFont = g_fonts.get_default_font();
		loadText->color = COLOR_RED;
		loadText->anchor = UIAnchorPos::create_single(0.5, 0.5);
		loadText->pivot_ofs = { 0.5,0.5 };

		layout = new GUIFullscreen;
		layout->add_this(loadText);

		eng->get_on_map_delegate().add(this, &GameTransitionUI::close);
	}
	static GameTransitionUI& get() {
		static GameTransitionUI inst;
		return inst;
	}
	void open() {
		eng->get_gui()->add_gui_panel_to_root(layout);
	}
	void close(bool b) {
		if (layout->parent)
			layout->parent->release_this(layout);
	}
	GUIText* loadText = nullptr;
	GUIFullscreen* layout = nullptr;
};

class GUIButtonWithSound : public GUIButton
{
public:
	GUIButtonWithSound(const SoundFile* s) : s(s) {}
	const SoundFile* s = nullptr;
	void on_pressed(int x, int y, int b) override {
		if (b == 1 && s)
			isound->play_sound(s);
		GUIButton::on_pressed(x, y, b);
	}
	void on_released(int x, int y, int b) override {
		if (b == 1 && s)
			isound->play_sound(s);
		GUIButton::on_released(x, y, b);
	}
};
extern ConfigVar g_entry_level;
class MainMenuUILayout : public GUIFullscreen
{
public:
	static GUIButtonWithSound* create_button(const char* textstr, const SoundFile* s) {
		GUIButtonWithSound* button = new GUIButtonWithSound(s);
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
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, "map thisIsTheMap.tmap\n");
		GameTransitionUI::get().open();
	}

	void exit_game() {
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, "quit\n");
	}
	void open_editor() {
		//auto file = isound->load_sound_file("wind1.wav");
		//isound->play_sound(file);
		const char* cmd = string_format("start_ed Map %s\n", g_entry_level.get_string());
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, cmd);
	}

	MainMenuUILayout() {
		auto vbox = new GUIVerticalBox;
		vbox->ls_position = { 100,100 };
		vbox->ls_sz = { 200,500 };

		const SoundFile* s = GetAssets().find_global_sync<SoundFile>("switch2.wav").get();

		GUIButton* b = create_button("PLAY",s);
		b->on_selected.add(this, &MainMenuUILayout::start_game);
		vbox->add_this(b);

		b = create_button("EXIT GAME",s);
		b->on_selected.add(this, &MainMenuUILayout::exit_game);
		vbox->add_this(b);

		b = create_button("OPEN EDITOR",s);
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

		eng->set_game_focused(false);	// disable mouse capture
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
