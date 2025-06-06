#pragma once

#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"
#include "Framework/Reflection2.h"
#include <vector>
#include <memory>

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

struct Serializer
{
	STRUCT_BODY()
};

struct SomeStruct
{
	STRUCT_BODY();
	REF int x = 0;
	REF int y = 0;
	
	REF void serialize(Serializer& s);
};

// Represents a drawable item in the world
class MeshComponent : public Component
{
public:
	CLASS_BODY(MeshComponent);

	MeshComponent();
	~MeshComponent() override;

	void pre_start() final;
	void start() final;
	void update() final;
	void end() final;
	void on_changed_transform() final;
	void editor_on_change_property() final;
	void on_sync_render_data() final;

	void set_model_str(const char* model_path);
	REF void set_model(Model* model);
	const Model* get_model() const;

	void set_animator_class(const ClassTypeInfo* ti);
	//REF void set_animation_graph(Animation_Tree_CFG* tree);
	const Animation_Tree_CFG* get_animation_tree() const;
	AnimatorInstance* get_animator_instance() const {
		return animator.get();
	}

	bool get_is_visible() const {
		return is_visible;
	}

	REF void set_is_visible(bool b) {
		is_visible = b;
		sync_render_data();
	}
	bool get_casts_shadows() const {
		return cast_shadows;
	}
	void set_casts_shadows(bool b) {
		cast_shadows = b;
		sync_render_data();
	}
	bool get_is_skybox() const {
		return is_skybox;
	}
	void set_is_skybox(bool b) {
		is_skybox = b;
		sync_render_data();
	}

	REFLECT(name = "set_material");
	void set_material_override(const MaterialInstance* mi);
	const MaterialInstance* get_material_override() const;

	glm::mat4 get_ls_transform_of_bone(StringName bone) const;
	int get_index_of_bone(StringName bone) const;

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final;
#endif

private:
	//REF Model* model = nullptr;

	REF AssetPtr<Model> model;		// the model of this
	
	//REF AssetPtr<Animation_Tree_CFG> animator_tree;
	
	REF bool is_visible = true;		// disables drawing
	REF bool cast_shadows = true;	// does model cast shadows
	REF bool is_skybox = false;		// is this mesh a skybox? used for skylight emission
	
	// array of material overrides for the model
	REF std::vector<AssetPtr<MaterialInstance>> eMaterialOverride;

	void update_animator_instance();
	std::unique_ptr<AnimatorInstance> animator;
	handle<Render_Object> draw_handle;
};