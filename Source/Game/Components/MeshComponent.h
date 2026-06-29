#pragma once

#include "Game/EntityComponent.h"

#include "Framework/Reflection2.h"
#include <vector>
#include <memory>
#include "Framework/StructReflection.h"
#include "Assets/IAsset.h"
#include "Framework/Handle.h"

class PhysicsFilterPresetBase;
class Model;
class AnimatorInstance;
struct Render_Object;
class PhysicsActor;
class Animation_Tree_CFG;
class MaterialInstance;
class RigidbodyComponent;
class MeshBuilderComponent;
class AnimatorObject;
class agBuilder;

struct LightmapCoords
{
	STRUCT_BODY();
	glm::vec4 to_vec4() const { return glm::vec4(x, y, xofs, yofs); }
	REF float x = 0;
	REF float y = 0;
	REF float xofs = 0;
	REF float yofs = 0;
};

class MeshComponent : public Component
{
public:
	CLASS_BODY(MeshComponent);

	MeshComponent();
	~MeshComponent() override;
	void start() final;
	void update() final;
	void stop() final;
	void on_changed_transform() final;
	void refresh_after_model_reload(Model* reloaded) final;
#ifdef EDITOR_BUILD
	void editor_on_change_property() final;
#endif // EDITOR_BUILD

	void on_sync_render_data() final;
	void set_model_str(const char* model_path);
	REF void set_model(Model* model);
	REF const Model* get_model() const;
	REF AnimatorObject* get_animator() const { return animator.get(); }
	REF AnimatorObject* create_animator(agBuilder* data);
	void release_animator();

	bool get_is_visible() const { return is_visible; }
	bool get_nav_static() const { return nav_static; }
	bool get_casts_shadows() const { return cast_shadows; }
	bool get_is_skybox() const { return is_skybox; }
	REF void set_material_override(const MaterialInstance* mi);
	REF const MaterialInstance* get_material_override() const;
	glm::mat4 get_ls_transform_of_bone(StringName bone) const;
	int get_index_of_bone(StringName bone) const;
	REF void set_is_visible(bool b) {
		is_visible = b;
		sync_render_data();
	}
	REF void set_casts_shadows(bool b) {
		cast_shadows = b;
		sync_render_data();
	}
	void set_is_skybox(bool b) {
		is_skybox = b;
		sync_render_data();
	}
#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final;
	void set_lightmapped(LightmapCoords coords);
	void set_not_lightmapped();
	void set_static_probe_lit(int index);
#endif
	void set_ignore_baking(bool ignore) { this->ignore_in_baking = ignore; }
	void set_ignore_cubemap_view(bool ignore) { this->ignore_in_cubemap = ignore; }
	void set_add_collision(bool add_col) { this->add_collision_if_available = true; }
	bool get_add_collision() const { return add_collision_if_available; }

	// public so the static-prop bake (free function below) can call it without becoming a friend.
	void populate_render_object(struct Render_Object& out, const glm::mat4& ws_transform) const;

private:
	void update_physics_mesh();

	REF AssetPtr<Model> model;
	REF bool is_visible = true;
	REF bool cast_shadows = true;
	REF bool is_skybox = false;
	REF float dist_cull_percentage = 0.0;
	// If true, then it will check the model for collision. If the model has collision and there isnt a meshcomponent
	// already, then it all create one. This only works on level load
	REF bool add_collision_if_available = true;
	REF bool ignore_in_baking = false;
	REF bool ignore_in_cubemap = false;
	// Include this mesh's triangles when baking the navmesh. Defaults true for static level geometry.
	REF bool nav_static = true;
	REFLECT(hide);
	bool lightmapped = false;
	REFLECT(hide);
	LightmapCoords lmCoords;
	REFLECT(hide);
	bool static_probe_lit = false;

	REF bool sort_first = false;

	REF std::vector<AssetPtr<MaterialInstance>> eMaterialOverride;
	std::unique_ptr<AnimatorObject> animator;
	handle<Render_Object> draw_handle;

	void update_animator_instance();
};

// Bake the render side of a static MeshComponent into the scene without keeping the Component alive.
// Used by Level's static-prop strip path: caller registers the handle, then frees the Entity+Component.
// Lifetime: caller owns the returned handle and must release via idraw->get_scene()->remove_obj.
handle<Render_Object> bake_static_meshcomponent_render(const MeshComponent& mc, const glm::mat4& ws_transform);

// this is just for previewing in the editor, use AnimatorObject on the MeshComponent for actual animation
class AnimationSeqAsset;
class agEvaluateClip;
class AnimPreviewComponent : public Component
{
public:
	CLASS_BODY(AnimPreviewComponent);
	AnimPreviewComponent() { set_call_init_in_editor(true); }
	void start() final;
	void update() final;
	void stop() final;
#ifdef EDITOR_BUILD
	void editor_on_change_property() final;
#endif

	agEvaluateClip* eval = nullptr;
	void update_mesh_component();
	int force_frame = -1;
	bool wants_force_frame = false;
	REF const Model* model = nullptr;
	REF const AnimationSeqAsset* asset = nullptr;
};