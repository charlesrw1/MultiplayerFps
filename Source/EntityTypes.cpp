#include "EntityTypes.h"
#include "Framework/Factory.h"

CLASS_IMPL(Entity);

CLASS_IMPL(Door);
CLASS_IMPL(Grenade);
CLASS_IMPL(NPC);


class Entity2;
CLASS_H(EntityComponent, ClassBase)
public:
	virtual void tick();
private:
	Entity2* entity_owner = nullptr;
	EntityComponent* attached_parent = nullptr;
	StringName attached_name;
	std::vector<EntityComponent*> children;
	std::vector<StringName> tags;
};
int i = sizeof(EntityComponent);

CLASS_H(MeshComponent, EntityComponent)
	// handles the rendering part
	// handle physics(both ragdoll,static, dynamic)
	// handles animating
};
CLASS_H(BoxComponent, EntityComponent)
	// a physics box collider
};
CLASS_H(CapsuleComponent, EntityComponent)
// a physics capsule collider
};
CLASS_H(SphereComponent, EntityComponent)
// a physics sphere collider
};
CLASS_H(AudioComponent, EntityComponent)
	// a component to play audio
};


#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionRegisterDefines.h"
CLASS_H(Entity2, ClassBase)
public:
	// these are added dynamically or added in the schema editor
	std::vector<unique_ptr<EntityComponent>> dynamic_components;
	// pointers to all components, both native + dynamic
	std::vector<EntityComponent*> components;
};
#define REG_COMPONENT(name, extra) // "Root","P=Mesh;S=SocketName"
CLASS_H(Player2, Entity2)

// all the movement related variables (or store these elsewhere, depends)
MeshComponent mesh;
CapsuleComponent collider;

AudioComponent footsteps;

static const PropertyInfoList* get_props() {
	START_PROPS(Player2)
		REG_COMPONENT(mesh),
		REG_COMPONENT(collider),
		REG_COMPONENT(footsteps),
	END_PROPS(Player2)
}

};