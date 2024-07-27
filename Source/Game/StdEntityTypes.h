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

CLASS_H(PointLightEntity,Entity)
public:
	PointLightEntity() {
		PointLight = create_sub_component<PointLightComponent>("PointLight");
		root_component = PointLight;
	}
	static const PropertyInfoList* get_props() = delete;
	PointLightComponent* PointLight = nullptr;
};
CLASS_H(SpotLightEntity, Entity)
public:
	SpotLightEntity() {
		SpotLight = create_sub_component<SpotLightComponent>("SpotLight");
		root_component = SpotLight;
	}
	static const PropertyInfoList* get_props() = delete;
	SpotLightComponent* SpotLight = nullptr;
};
CLASS_H(SunLightEntity, Entity)
public:
	SunLightEntity() {
		Sun = create_sub_component<SunLightComponent>("Sun");
		root_component = Sun;
	}
	static const PropertyInfoList* get_props() = delete;
	SunLightComponent* Sun = nullptr;
};



// represents a prefabed level instance
CLASS_H(PrefabEntity, Entity)
public:
};

// multiple prefabs with some randomization options
CLASS_H(PrefabSelection,Entity)
public:
};
