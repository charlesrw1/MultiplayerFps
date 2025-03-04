#pragma once
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Level.h"

NEWCLASS(BEU_Manager, EntityComponent)
public:
	static BEU_Manager* instance;
	void pre_start() final {
		assert(!instance);
		instance = this;
	}
	void end() final {
		instance = nullptr;
	}
	void start() final {

	}
	void update() final {

	}

	REFLECT();
	EntityPtr start_entity;
};

NEWCLASS(BEU_Enemy, EntityComponent)
public:
	void start() final {

	}
	void update() final {

	}
};

NEWCLASS(BEU_Player, EntityComponent)
public:
	void start() final {

	}
	void update() final {

	}
};
