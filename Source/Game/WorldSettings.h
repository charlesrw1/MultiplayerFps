#pragma once
#include "Entity.h"
#include "Framework/ClassTypePtr.h"
#include "BasePlayer.h"
#include "GameMode.h"
// A pure data container entity for storing level specific information
// Cruicially, it stores the "game mode" (yes im copying unreal here ;) )
// A Player class is also spawned per player. For a menu level, you can set this to be a null class to skip any functionality
// Override to add any more special data saved per level that can then be referenced by the game_mode or other classes
// This persists through the levels lifetime, but shouldn't be ticked
CLASS_H(WorldSettings, Entity)
public:
	WorldSettings();

	static const PropertyInfoList* get_props()  {
		START_PROPS(WorldSettings)
			REG_CLASSTYPE_PTR(gamemode_type,PROP_DEFAULT),
			REG_CLASSTYPE_PTR(player_type,PROP_DEFAULT),
		END_PROPS(WorldSettings)
	}

	ClassTypePtr<GameMode> gamemode_type;
	ClassTypePtr<PlayerBase> player_type;
};