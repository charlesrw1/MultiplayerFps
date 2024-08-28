#pragma once

#include "Game/EntityComponent.h"
#include "Physics/ChannelsAndPresets.h"
#include "Framework/ClassTypePtr.h"
#include "Render/RenderObj.h"

class PhysicsActor;
class MeshBuilder;


class MeshBuilderComponent;
CLASS_H(PhysicsComponentBase, EntityComponent)
public:
	~PhysicsComponentBase();
	PhysicsComponentBase();

	bool disable_physics = false;
	ClassTypePtr<PhysicsFilterPresetBase> physics_preset;
	bool simulate_physics = false;
	bool is_trigger = false;
	bool is_static = false;
	bool send_hit = false;
	bool send_overlap = true;

	static const PropertyInfoList* get_props() {
		START_PROPS(PhysicsComponentBase)
			REG_BOOL(is_trigger, PROP_DEFAULT, "0"),
			REG_BOOL(send_hit, PROP_DEFAULT, "0"),
			REG_BOOL(send_overlap, PROP_DEFAULT, "1"),
			REG_CLASSTYPE_PTR(physics_preset, PROP_DEFAULT)
		END_PROPS(PhysicsComponentBase)
	};

	void on_deinit() override;
	void on_changed_transform() override;
protected:
	PhysicsActor* actor = nullptr;

	MeshBuilderComponent* editor_view = nullptr;
};

CLASS_H(CapsuleComponent, PhysicsComponentBase)
public:
	~CapsuleComponent() override {}

	static const PropertyInfoList* get_props() {
		START_PROPS(CapsuleComponent)
			REG_FLOAT(height,PROP_DEFAULT,"2.0"),
			REG_FLOAT(radius,PROP_DEFAULT,"0.5"),
		END_PROPS(CapsuleComponent)
	};
	void on_init() override;

	float height = 2.f;
	float radius = 0.5;
};
CLASS_H(BoxComponent, PhysicsComponentBase)
public:
	~BoxComponent() override {}


	void on_init() override;

	static const PropertyInfoList* get_props() {
		START_PROPS(BoxComponent)
			REG_VEC3(side_len, PROP_DEFAULT),
		END_PROPS(BoxComponent)
	};
	glm::vec3 side_len{1.f,1.f,1.f};
};
CLASS_H(SphereComponent, PhysicsComponentBase)
public:
	~SphereComponent() override {}

	void on_init() override;

	static const PropertyInfoList* get_props() {
		START_PROPS(SphereComponent)
			REG_FLOAT(radius, PROP_DEFAULT, "1.0"),
		END_PROPS(SphereComponent)
	};
	float radius = 1.f;
};