#pragma once
#include "Framework/ClassBase.h"

// yep definitely not copying unreal...
class Player;
CLASS_H(GameMode, ClassBase)
public:
	virtual ~GameMode() {}
	// called post unserialization, before anything has reg'd 
	// can handle any loading from a save file here
	virtual void init() {}
	// called after all entities start()'d
	virtual void start() {}
	// called when map is getting removed
	virtual void end() {}

	// called every frame
	virtual void tick() {}

	// called when spawning a player for a client slot
	virtual void on_player_create(int slot, Player* player) {}
};