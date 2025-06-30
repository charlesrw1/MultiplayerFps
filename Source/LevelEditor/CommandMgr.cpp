#ifdef EDITOR_BUILD
#include "CommandMgr.h"
#include <SDL2/SDL_events.h>
UndoRedoSystem::UndoRedoSystem() {
	hist.resize(HIST_SIZE, nullptr);
}
void UndoRedoSystem::on_key_event(const SDL_KeyboardEvent& key)
{
	if (key.keysym.scancode == SDL_SCANCODE_Z && key.keysym.mod & KMOD_CTRL)
		undo();
}

void UndoRedoSystem::clear_all() {
	for (int i = 0; i < hist.size(); i++) {
		delete hist[i];
		hist[i] = nullptr;
	}
}

// returns number of errord commands 
//

int UndoRedoSystem::execute_queued_commands() {

	int errored_command_count = 0;
	for (auto&[c,callback] : queued_commands) {
		
		if (!c->is_valid()) {
			sys_print(Warning, "execute_queued_commands: command not valid %s\n", c->to_string().c_str());
			if(callback)
				callback(false);
			delete c;
			errored_command_count++;
			continue;
		}
		sys_print(Debug, "execute_queued_commands: executing: %s\n", c->to_string().c_str());
		try {
			c->execute();
			if(callback)
				callback(true);

			if (hist[index]) {
				delete hist[index];
			}
			hist[index] = c;
			index += 1;
			index %= HIST_SIZE;
			eng->log_to_fullscreen_gui(Info, c->to_string().c_str());
		}
		catch (std::runtime_error er) {
			sys_print(Error, "execute_queued_commands: command threw exception: %s\n", er.what());
			errored_command_count++;

			if (callback)
				callback(false);
			delete c;
		}
	}
	bool had_commands = !queued_commands.empty();
	queued_commands.clear();
	if (had_commands)
		on_command_execute_or_undo.invoke();

	return errored_command_count;
}
void UndoRedoSystem::undo() {
	index -= 1;
	if (index < 0) index = HIST_SIZE - 1;
	if (hist[index]) {

		sys_print(Debug, "Undoing: %s\n", hist[index]->to_string().c_str());

		eng->log_to_fullscreen_gui(Info, "Undo");

		hist[index]->undo();
		delete hist[index];
		hist[index] = nullptr;

		on_command_execute_or_undo.invoke();
	}
	else {
		eng->log_to_fullscreen_gui(Warning, "Nothing to undo");

		sys_print(Debug, "nothing to undo\n");
	}
}
#endif