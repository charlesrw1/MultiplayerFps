#pragma once
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Level.h"

NEWCLASS(BEU_Manager, Entity)
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

NEWCLASS(BEU_Enemy, Entity)
public:
	void start() final {

	}
	void update() final {

	}
};

NEWCLASS(BEU_Player, Entity)
public:
	void start() final {

	}
	void update() final {

	}
};
