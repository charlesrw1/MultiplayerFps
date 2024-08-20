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
			REG_BOOL(isTrigger,PROP_DEFAULT,"0"),
			REG_BOOL(sendHit, PROP_DEFAULT, "0"),
			REG_BOOL(sendOverlap, PROP_DEFAULT, "1"),
			REG_CLASSTYPE_PTR(physicsPreset,PROP_DEFAULT)
		END_PROPS(CapsuleComponent)
	};
	void on_init() override;
	void on_deinit() override;
	void on_changed_transform() override;

	float height = 2.f;
	float radius = 0.5;
	ClassTypePtr<PhysicsFilterPresetBase> physicsPreset;
	bool isTrigger = false;
	bool sendHit = false;
	bool sendOverlap = true;
private:
	PhysicsActor* actor = nullptr;
};
CLASS_H(BoxComponent, EntityComponent)
public:
};