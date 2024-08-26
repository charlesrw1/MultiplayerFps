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
	void update() override;
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
	bool e_animate_in_editor = false;
	void editor_on_change_property() override;
#endif // !RUNTIME

	static const PropertyInfoList* get_props();

	void set_material_override(const MaterialInstance* mi);

	// physics settings
	bool disable_physics = false;	// component disables all physics
	ClassTypePtr<PhysicsFilterPresetBase> physicsPreset;	//  preset to determine collision masks (like an enum)
	
	bool simulate_physics = false;		// if true, then object is a DYNAMIC object driven by the physics simulation
	
	bool is_static = true;				// if true, then the object is a STATIC object driven that cant ever move
										// if false, then this object is KINEMATIC if simulate_physics is false or DYNAMIC if its true
										// isStatic and simulate_physics is illogical so it falls back to isStatic in that case

	bool is_trigger = false;			// if true, then the objects shapes are treated like triggers and sends OVERLAP events
										// for a generic static trigger box, use with is_static = true
	
	bool send_overlap = false;			// if true on both objects, then a overlap event will be sent (one of the objects has to be a trigger object)
	
	bool send_hit = false;				// if true on both objects, then a hit event will be sent when the 2 objects hit each other in the simulation
private:
	void update_handle();

	AssetPtr<Model> model;
	std::vector<AssetPtr<MaterialInstance>> eMaterialOverride;
	AssetPtr<Animation_Tree_CFG> animator_tree;
	std::unique_ptr<AnimatorInstance> animator;
	handle<Render_Object> draw_handle;


	PhysicsActor* physActor = nullptr;
};