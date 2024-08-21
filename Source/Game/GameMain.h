#pragma once

#include "Framework/ClassBase.h"

// once again like unreal :)

// represents a singleton of the actual game that lasts from when the game starts to when it ends
// it gets created/deleted when going from game to editor states
// set what classname to use in the config_var g_gamemain_class defined in Main.cpp

CLASS_H(GameMain, ClassBase)
public:
	virtual void init() {}
	virtual void shutdown() {}
};