#pragma once

#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"
#include "Framework/Reflection2.h"
#include <vector>

GENERATED_CLASS_INCLUDE("Render/Model.h");
GENERATED_CLASS_INCLUDE("Render/MaterialPublic.h");
GENERATED_CLASS_INCLUDE("Animation/AnimationTreePublic.h");

class PhysicsFilterPresetBase;
class Model;
class AnimatorInstance;
struct Render_Object;
class PhysicsActor;
class Animation_Tree_CFG;
class MaterialInstance;
class RigidbodyComponent;
class MeshBuilderComponent;
NEWCLASS(MeshComponent, EntityComponent)
public:
	MeshComponent();
	~MeshComponent() override;

	void pre_start() override;
	void start() override;
	void update() override;
	void end() override;
	void on_changed_transform() override;
	void editor_on_change_property() override;

	void set_model_str(const char* model_path);
	REFLECT();
	void set_model(Model* model);
	const Model* get_model() const;

	void set_animator_class(const ClassTypeInfo* ti);
	REFLECT();
	void set_animation_graph(Animation_Tree_CFG* tree);
	const Animation_Tree_CFG* get_animation_tree() const;
	AnimatorInstance* get_animator_instance() const {
		return animator;
	}

	bool get_is_visible() const {
		return is_visible;
	}

	REFLECT();
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

	REFLECT(name = "set_material");
	void set_material_override(const MaterialInstance* mi);
	const MaterialInstance* get_material_override() const;

	glm::mat4 get_ls_transform_of_bone(StringName bone) const;
	int get_index_of_bone(StringName bone) const;

private:
	REFLECT();
	AssetPtr<Model> model;
	REFLECT();
	AssetPtr<Animation_Tree_CFG> animator_tree;
	REFLECT();
	bool is_visible = true;
	REFLECT();
	bool cast_shadows = true;
	REFLECT();
	bool is_skybox = false;
	REFLECT();
	std::vector<AssetPtr<MaterialInstance>> eMaterialOverride;

	void update_handle();
	void update_animator_instance();

	AnimatorInstance* animator = nullptr;	// owning ptr
	handle<Render_Object> draw_handle;
};