#pragma once
#include "Game/EntityComponent.h"

CLASS_H(BikeAI, EntityComponent)
public:
	void on_init() override;
	void update() override;
	void on_deinit() override;
};