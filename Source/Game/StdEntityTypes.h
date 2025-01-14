#pragma once
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/LightComponents.h"
#include "Game/Components/DecalComponent.h"

// a static mesh
CLASS_H(StaticMeshEntity, Entity)
public:
	StaticMeshEntity() {
		Mesh = construct_sub_component<MeshComponent>("Mesh");
	}
	MeshComponent* Mesh = nullptr;
	static const PropertyInfoList* get_props() = delete;
};

CLASS_H(PointLightEntity,Entity)
public:
	PointLightEntity() {
		construct_sub_component<PointLightComponent>("PointLight");
	}
	static const PropertyInfoList* get_props() = delete;
};
CLASS_H(SpotLightEntity, Entity)
public:
	SpotLightEntity() {
		construct_sub_component<SpotLightComponent>("SpotLight");
	}
	static const PropertyInfoList* get_props() = delete;
};
CLASS_H(SunLightEntity, Entity)
public:
	SunLightEntity() {
		Sun = construct_sub_component<SunLightComponent>("Sun");
	}
	SunLightComponent* Sun = nullptr;
	static const PropertyInfoList* get_props() = delete;
};
CLASS_H(DecalEntity, Entity)
public:
	DecalEntity() {
		construct_sub_component<DecalComponent>("Decal");
	}
	static const PropertyInfoList* get_props() = delete;
};



// represents a prefabed level instance
CLASS_H(PrefabEntity, Entity)
public:
};

// multiple prefabs with some randomization options
CLASS_H(PrefabSelection,Entity)
public:
};
