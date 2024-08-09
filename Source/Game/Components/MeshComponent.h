#pragma once

#include "Game/EntityComponent.h"

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


	bool simulate_physics = false;
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

	std::unique_ptr<AnimatorInstance> animator;
	handle<Render_Object> draw_handle;
	PhysicsActor* physics_actor = nullptr;

	AssetPtr<Model> model;
	AssetPtr<Animation_Tree_CFG> animator_tree;

#ifndef RUNTIME
	std::vector<AssetPtr<MaterialInstance>> eMaterialOverride;
#endif RUNTIME
	std::vector<AssetPtr<MaterialInstance>> MaterialOverride_compilied;
};