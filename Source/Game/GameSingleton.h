#pragma once

#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"

// use "ADD_GAME_SINGLETON <>" command in the cfg script to add these
// they get loaded once per game, stores persistent state
// they get refreshed when going in/out of the editor
NEWCLASS(IGameSingleton, ClassBase)
public:
	virtual void on_start_game() {}
	virtual void on_leave_game() {}
};