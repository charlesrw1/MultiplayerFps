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
class RigidbodyComponent;
CLASS_H(MeshComponent, EntityComponent)
public:
	MeshComponent();
	~MeshComponent() override;

	void start() override;
	void update() override;
	void end() override;

	void set_model(const char* model_path);
	void set_model(Model* model);
	const Model* get_model() const;

	template<typename T>
	void set_animator_class() {
		set_animator_class(&T::StaticType);
	}
	void set_animator_class(const ClassTypeInfo* ti);
	void set_animation_graph(const char* graph);

	void on_changed_transform() override;


	bool visible = true;
	bool cast_shadows = true;
	bool is_skybox = false;

#ifndef RUNTIME
	bool e_animate_in_editor = false;
	void editor_on_change_property() override;
#endif // !RUNTIME

	static const PropertyInfoList* get_props();

	void set_material_override(const MaterialInstance* mi);

	glm::mat4 get_ls_transform_of_bone(StringName bone) const;
private:
	void update_handle();

	AssetPtr<Model> model;
	std::vector<AssetPtr<MaterialInstance>> eMaterialOverride;
	AssetPtr<Animation_Tree_CFG> animator_tree;
	std::unique_ptr<AnimatorInstance> animator;
	handle<Render_Object> draw_handle;
};