#pragma once
#include <string>
#include <vector>
#include "Framework/Util.h"
#include "GameEnginePublic.h"

class Command
{
public:
	virtual ~Command() {}
	virtual void execute() = 0;
	virtual void undo() = 0;
	virtual std::string to_string() = 0;
	virtual bool is_valid() { return true; }
};

struct SDL_KeyboardEvent;
class UndoRedoSystem
{
public:
	UndoRedoSystem();

	void on_key_event(const SDL_KeyboardEvent& k);

	void clear_all() {
		for (int i = 0; i < hist.size(); i++) {
			delete hist[i];
			hist[i] = nullptr;
		}
	}
	
	void execute_queued_commands() {

		for (auto c : queued_commands) {
	
			if (!c->is_valid()) {
				sys_print(Warning, "command not valid %s\n", c->to_string().c_str());
				delete c;
				continue;
			}

			if (hist[index]) {
				delete hist[index];
			}
			hist[index] = c;
			index += 1;
			index %= HIST_SIZE;

			sys_print(Debug, "Executing: %s\n", c->to_string().c_str());
			c->execute();
			eng->log_to_fullscreen_gui(Info, c->to_string().c_str());
		}

		queued_commands.clear();
	}

	void add_command(Command* c) {
		queued_commands.push_back(c);
	}
	void undo() {
		index -= 1;
		if (index < 0) index = HIST_SIZE - 1;
		if (hist[index]) {

			sys_print(Debug,"Undoing: %s\n", hist[index]->to_string().c_str());

			eng->log_to_fullscreen_gui(Info, "Undo");

			hist[index]->undo();
			delete hist[index];
			hist[index] = nullptr;
		}
		else {
			eng->log_to_fullscreen_gui(Warning, "Nothing to undo");

			sys_print(Debug,"nothing to undo\n");
		}
	}

	std::vector<Command*> queued_commands;

	const int HIST_SIZE = 128;
	int index = 0;
	std::vector<Command*> hist;
};
