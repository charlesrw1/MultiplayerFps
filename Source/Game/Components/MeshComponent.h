#pragma once

#include "Game/EntityComponent.h"
#include "Framework/ClassTypePtr.h"
class PhysicsFilterPresetBase;
class Model;
class AnimatorInstance;
struct Render_Object;
class PhysicsActor;
class Animation_Tree_CFG;
class MaterialInstance;
CLASS_H(MeshComponent, EntityComponent)
public:
	MeshComponent();
	~MeshComponent() override;

	void on_init() override;
	void on_tick() override;
	void on_deinit() override;

	void set_model(const char* model_path);
	void set_model(Model* model);

	template<typename T>
	void set_animator_class() {
		set_animator_class(&T::StaticType);
	}
	void set_animator_class(const ClassTypeInfo* ti);

	void on_changed_transform() override;


	bool visible = true;
	bool cast_shadows = true;
	bool is_skybox = false;

#ifndef RUNTIME
	bool eAnimateInEditor = false;
	void editor_on_change_property() override;
#endif // !RUNTIME

	static const PropertyInfoList* get_props();

	void set_material_override(const MaterialInstance* mi);
private:
	void update_handle();

	AssetPtr<Model> model;
	std::vector<AssetPtr<MaterialInstance>> eMaterialOverride;
	AssetPtr<Animation_Tree_CFG> animator_tree;
	std::unique_ptr<AnimatorInstance> animator;
	handle<Render_Object> draw_handle;


	ClassTypePtr<PhysicsFilterPresetBase> physicsPreset;
	bool simulate_physics = false;

	PhysicsActor* physics_actor = nullptr;
};