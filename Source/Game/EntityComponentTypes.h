#pragma once

#include <memory>
#include "Framework/Handle.h"
#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"

CLASS_H(EmptyComponent, EntityComponent)
public:
	~EmptyComponent() override {}
};


class Model;
class Animator_Tree_CFG;
class PhysicsActor;
class Animation_Tree_CFG;
class AnimatorInstance;
class Render_Object;
class Material;
CLASS_H(MeshComponent, EntityComponent)
public:
	MeshComponent();
	~MeshComponent() override;

	void on_init() override;
	void on_tick() override;
	void on_deinit() override;

	void set_model(const char* model_path);

	template<typename T>
	void set_animator_class() {
		set_animator_class(&T::StaticType);
	}
	void set_animator_class(const ClassTypeInfo* ti);

	bool simulate_physics = false;
	bool is_hidden = false;

#ifndef RUNTIME
	bool eAnimateInEditor = false;
#endif // !RUNTIME

	static const PropertyInfoList* get_props();

private:
	std::unique_ptr<AnimatorInstance> animator;
	handle<Render_Object> draw_handle;
	PhysicsActor* physics_actor = nullptr;

	AssetPtr<Model> model;
	AssetPtr<Animation_Tree_CFG> animator_tree;

#ifndef RUNTIME
	std::vector<AssetPtr<Material>> eMaterialOverride;
#endif RUNTIME
	std::vector<AssetPtr<Material>> MaterialOverride_compilied;
};

CLASS_H(CapsuleComponent, EntityComponent)
public:

};
CLASS_H(BoxComponent, EntityComponent)
public:

};