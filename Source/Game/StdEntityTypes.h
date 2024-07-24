#pragma once
#include "Entity.h"
#include "EntityComponentTypes.h"
// a static mesh
CLASS_H(StaticMeshEntity, Entity)
public:
	StaticMeshEntity() {
		Mesh = create_sub_component<MeshComponent>("Mesh");
		root_component = Mesh;
	}
	static const PropertyInfoList* get_props() = delete;

	MeshComponent* Mesh = nullptr;

};

// represents a prefabed level instance
CLASS_H(PrefabEntity, Entity)
public:
};

// multiple prefabs with some randomization options
CLASS_H(PrefabSelection,Entity)
public:
};

CLASS_H(PointLightEntity, Entity)
public:
};
CLASS_H(SpotLightEntity, Entity)
public:
};