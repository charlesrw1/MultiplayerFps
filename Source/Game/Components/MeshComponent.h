#pragma once

#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"
#include "Framework/Reflection2.h"
#include <vector>
#include <memory>
#include "Framework/StructReflection.h"

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

struct LightmapCoords {
	STRUCT_BODY();
	glm::vec4 to_vec4() const {
		return glm::vec4(x, y, xofs, yofs);
	}
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
	void pre_start() final;
	void start() final;
	void update() final;
	void stop() final;
	void on_changed_transform() final;
#ifdef EDITOR_BUILD
	void editor_on_change_property() final;
#endif // EDITOR_BUILD

	void on_sync_render_data() final;
	void set_model_str(const char* model_path);
	REF void set_model(Model* model);
	REF const Model* get_model() const;
	REF AnimatorObject* get_animator() const { 
		return animator.get(); 
	}
	REF AnimatorObject* create_animator(agBuilder* data);

	bool get_is_visible() const { return is_visible; }
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

private:
	REF AssetPtr<Model> model;
	REF bool is_visible = true;
	REF bool cast_shadows = true;
	REF bool is_skybox = false;
	REFLECT(hide);
	bool lightmapped = false;
	REFLECT(hide);
	LightmapCoords lmCoords;
	REFLECT(hide);
	bool static_probe_lit = false;

	REF std::vector<AssetPtr<MaterialInstance>> eMaterialOverride;
	std::unique_ptr<AnimatorObject> animator;
	handle<Render_Object> draw_handle;

	void update_animator_instance();
};

// hack for property grid ui
struct AnimPreviewInfoUi {
	STRUCT_BODY();
};

// this is just for previewing in the editor, use AnimatorObject on the MeshComponent for actual animation
class AnimationSeqAsset;
class AnimPreviewComponent : public Component {
public:
	CLASS_BODY(AnimPreviewComponent);
	void pre_start() final;
	void start() final;
	void update() final;
	void stop() final;
#ifdef EDITOR_BUILD
	void editor_on_change_property() final;
#endif
private:
	REF const AnimationSeqAsset* asset = nullptr;
	REF AnimPreviewInfoUi info;
};