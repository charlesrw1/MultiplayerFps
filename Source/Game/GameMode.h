#pragma once
#include "Framework/ClassBase.h"

// yep definitely not copying unreal...
class Player;
CLASS_H(GameMode, ClassBase)
public:
	// called post unserialization, before anything has reg'd 
	// can handle any loading from a save file here
	virtual void init() = 0;
	// called after entities reg'ing, but before any have start()'d
	virtual void start() = 0;
	// called when map is getting removed
	virtual void end() = 0;

	// called every frame
	virtual void tick() = 0;

	// called when spawning a player for a client slot
	virtual void on_player_create(int slot, Player* player) = 0;
};