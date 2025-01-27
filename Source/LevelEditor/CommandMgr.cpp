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
#endif