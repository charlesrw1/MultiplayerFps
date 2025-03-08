#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <vector>
#include "Framework/Util.h"
#include "GameEnginePublic.h"
#include <stdexcept>
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
	// returns number of errord commands
	int execute_queued_commands() {

		int errored_command_count = 0;
		for (auto c : queued_commands) {
	
			if (!c->is_valid()) {
				sys_print(Warning, "command not valid %s\n", c->to_string().c_str());
				delete c;
				errored_command_count++;
				continue;
			}
			sys_print(Debug, "Executing: %s\n", c->to_string().c_str());
			try {
				c->execute();

				if (hist[index]) {
					delete hist[index];
				}
				hist[index] = c;
				index += 1;
				index %= HIST_SIZE;
				eng->log_to_fullscreen_gui(Info, c->to_string().c_str());
			}
			catch (std::runtime_error er) {
				sys_print(Error, "Executing command failed: %s\n", er.what());
				errored_command_count++;
				delete c;
			}
		}

		queued_commands.clear();

		return errored_command_count;
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
#endif