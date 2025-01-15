#pragma once
#include <string>
#include <vector>
#include "Framework/Util.h"

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
	void add_command(Command* c) {

		if (!c->is_valid()) {
			sys_print(Warning, "command not valid %s\n", c->to_string().c_str());
			delete c;
			return;
		}

		if (hist[index]) {
			delete hist[index];
		}
		hist[index] = c;
		index += 1;
		index %= HIST_SIZE;

		sys_print(Debug,"Executing: %s\n", c->to_string().c_str());

		c->execute();
	}
	void undo() {
		index -= 1;
		if (index < 0) index = HIST_SIZE - 1;
		if (hist[index]) {

			sys_print(Debug,"Undoing: %s\n", hist[index]->to_string().c_str());


			hist[index]->undo();
			delete hist[index];
			hist[index] = nullptr;
		}
		else {
			sys_print(Debug,"nothing to undo\n");
		}
	}

	const int HIST_SIZE = 128;
	int index = 0;
	std::vector<Command*> hist;
};
