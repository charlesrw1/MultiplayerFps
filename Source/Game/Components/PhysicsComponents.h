#pragma once

#include "Game/EntityComponent.h"
#include "Physics/ChannelsAndPresets.h"
#include "Framework/ClassTypePtr.h"
class PhysicsActor;
CLASS_H(CapsuleComponent, EntityComponent)
public:
	static const PropertyInfoList* get_props() {
		START_PROPS(CapsuleComponent)
			REG_FLOAT(height,PROP_DEFAULT,"2.0"),
			REG_FLOAT(radius,PROP_DEFAULT,"0.5"),
			REG_BOOL(is_trigger,PROP_DEFAULT,"0"),
			REG_BOOL(send_hit, PROP_DEFAULT, "0"),
			REG_BOOL(send_overlap, PROP_DEFAULT, "1"),
			REG_CLASSTYPE_PTR(physics_preset,PROP_DEFAULT)
		END_PROPS(CapsuleComponent)
	};
	void on_init() override;
	void on_deinit() override;
	void on_changed_transform() override;

	float height = 2.f;
	float radius = 0.5;

	bool disable_physics = false;
	ClassTypePtr<PhysicsFilterPresetBase> physics_preset;
	bool simulate_physics = false;
	bool is_trigger = false;
	bool is_static = false;
	bool send_hit = false;
	bool send_overlap = true;
private:
	PhysicsActor* actor = nullptr;
};
CLASS_H(BoxComponent, EntityComponent)
public:
};