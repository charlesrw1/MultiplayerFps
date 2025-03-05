#pragma once
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Level.h"

namespace bu {
NEWCLASS(Manager, EntityComponent)
public:
	static Manager* instance;
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

NEWCLASS(Enemy, EntityComponent)
public:
	void start() final {

	}
	void update() final {

	}
};

NEWCLASS(Player, EntityComponent)
public:
	void start() final {

	}
	void update() final {

	}
};
}
