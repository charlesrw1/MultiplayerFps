#pragma once

#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"

class PhysicsFilterPresetBase;
class Model;
class AnimatorInstance;
struct Render_Object;
class PhysicsActor;
class Animation_Tree_CFG;
class MaterialInstance;
class RigidbodyComponent;
class MeshBuilderComponent;
CLASS_H(MeshComponent, EntityComponent)
public:
	MeshComponent();
	~MeshComponent() override;

	void pre_start() override;
	void start() override;
	void update() override;
	void end() override;
	void on_changed_transform() override;
	void editor_on_change_property() override;

	void set_model(const char* model_path);
	void set_model(Model* model);
	const Model* get_model() const;

	void set_animator_class(const ClassTypeInfo* ti);
	void set_animation_graph(Animation_Tree_CFG* tree);
	const Animation_Tree_CFG* get_animation_tree() const;
	AnimatorInstance* get_animator_instance() const {
		return animator;
	}

	bool get_is_visible() const {
		return is_visible;
	}
	void set_is_visible(bool b) {
		is_visible = b;
		update_handle();
	}
	bool get_casts_shadows() const {
		return cast_shadows;
	}
	void set_casts_shadows(bool b) {
		cast_shadows = b;
		update_handle();
	}
	bool get_is_skybox() const {
		return is_skybox;
	}
	void set_is_skybox(bool b) {
		is_skybox = b;
		update_handle();
	}

	static const PropertyInfoList* get_props();

	void set_material_override(const MaterialInstance* mi);
	const MaterialInstance* get_material_override() const;

	glm::mat4 get_ls_transform_of_bone(StringName bone) const;
	int get_index_of_bone(StringName bone) const;

private:
	bool is_visible = true;
	bool cast_shadows = true;
	bool is_skybox = false;

	void update_handle();
	void update_animator_instance();

	AssetPtr<Model> model;
	std::vector<AssetPtr<MaterialInstance>> eMaterialOverride;
	AssetPtr<Animation_Tree_CFG> animator_tree;
	AnimatorInstance* animator = nullptr;	// owning ptr
	handle<Render_Object> draw_handle;
};