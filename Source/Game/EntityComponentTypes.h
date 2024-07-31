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
class MaterialInstance;
class Texture;
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

#ifndef RUNTIME
	bool eAnimateInEditor = false;
	void editor_on_change_property(const PropertyInfo& property_) override;
#endif // !RUNTIME

	static const PropertyInfoList* get_props();

private:
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

CLASS_H(CapsuleComponent, EntityComponent)
public:
	static const PropertyInfoList* get_props() = delete;
};
CLASS_H(BoxComponent, EntityComponent)
public:

};


struct Render_Light;
CLASS_H(SpotLightComponent, EntityComponent)
public:
	void on_init() override;
	void on_deinit() override;
	void on_changed_transform() override;
	~SpotLightComponent() override;
	SpotLightComponent();


	static const PropertyInfoList* get_props();

	float cone_angle = 45.0;
	float inner_cone = 40.0;

	float radius = 20.f;
	glm::vec3 color = glm::vec3(1.f);

	void build_render_light(Render_Light& light);

	AssetPtr<Texture> cookie_asset;
	bool visible = true;
	handle<Render_Light> light_handle;
};

CLASS_H(PointLightComponent, EntityComponent)
public:
	void on_init() override;
	void on_deinit() override;
	void on_changed_transform() override;
	~PointLightComponent() override;


	void build_render_light(Render_Light& light);

	static const PropertyInfoList* get_props();

	float radius = 5.f;
	glm::vec3 color = glm::vec3(1.f);

	bool visible = true;
	handle<Render_Light> light_handle;
};

struct Render_Sun;
CLASS_H(SunLightComponent, EntityComponent)
public:
	void on_init() override;
	void on_deinit() override;
	void on_changed_transform() override;
	void build_render_light(Render_Sun& light);
	~SunLightComponent() override;

	static const PropertyInfoList* get_props();

	glm::vec3 color = glm::vec3(12.f);

	bool fit_to_scene = true;
	float log_lin_lerp_factor = 0.5;
	float max_shadow_dist = 80.f;
	float epsilon = 0.008f;
	float z_dist_scaling = 1.f;

	bool visible = true;
	handle<Render_Sun> light_handle;
};

#if 0
CLASS_H(TimeSeqComponent, EntityComponent)
public:
	void play();
	void stop();
	bool is_playing() const;
	float get_percentage() const;

	void on_init() override;

	// ticks the timeline if playing
	void on_tick() override;

	// gets a curve value
	float get_curve_value(StringName curve_name);

	// MulticastDelegate OnFinished;	called when timeline reaches the end
	// MulticastDelegate OnTick;		called every tick its playing
};
#endif