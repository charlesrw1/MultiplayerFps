#pragma once

#include "Game/Entity.h"
#include "Game/Components/PhysicsComponents.h"

CLASS_H(TriggerBox, Entity)
public:
	TriggerBox() {
		box = construct_sub_component<BoxComponent>("TriggerBox");

		box->is_trigger = true;
		box->simulate_physics = false;
		box->physics_preset.ptr = &PP_Trigger::StaticType;
	}
	static const PropertyInfoList* get_props() = delete;


	BoxComponent* box = nullptr;
};
CLASS_H(TriggerSphere, Entity)
public:
	TriggerSphere() {
		sphere = construct_sub_component<SphereComponent>("TriggerSphere");

		sphere->is_trigger = true;
		sphere->simulate_physics = false;
		sphere->physics_preset.ptr = &PP_Trigger::StaticType;
	}
	static const PropertyInfoList* get_props() = delete;

	SphereComponent* sphere = nullptr;
};