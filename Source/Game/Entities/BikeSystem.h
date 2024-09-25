#pragma once
#include "Game/Entity.h"
#include "Framework/Hashset.h"

class BikeAIComponent;
class BikeEntity;
CLASS_H(BikeAISystem, Entity)
public:
	void start() override;
	void update() override;


	hash_set<BikeAIComponent> bikeAIs;
	hash_set<BikeEntity> bikes;
};