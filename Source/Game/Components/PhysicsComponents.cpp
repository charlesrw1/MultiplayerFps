#include "PhysicsComponents.h"
#include "GameEnginePublic.h"
#include "Physics/Physics2.h"
#include "BillboardComponent.h"
#include "Assets/AssetDatabase.h"
#include "Game/Entity.h"
#include "Render/Texture.h"
#include "Framework/MeshBuilder.h"
#include "Render/DrawPublic.h"

CLASS_IMPL(PhysicsComponentBase);
CLASS_IMPL(BoxComponent);
CLASS_IMPL(CapsuleComponent);
CLASS_IMPL(SphereComponent);

CLASS_H(MeshBuilderComponent, EntityComponent)
public:
	MeshBuilderComponent() {
		dont_serialize_or_edit = true;
	}
	~MeshBuilderComponent() {
		assert(!editor_mb_handle.is_valid());
	}

	static const PropertyInfoList* get_props() = delete;
	void on_init() override {
		MeshBuilder_Object mbo;
		fill_out_struct(mbo);
		editor_mb_handle = idraw->get_scene()->register_meshbuilder(mbo);
	}
	void on_deinit() override {
		idraw->get_scene()->remove_meshbuilder(editor_mb_handle);
		editor_mb.Free();
	}
	void on_changed_transform() override {
		MeshBuilder_Object mbo;
		fill_out_struct(mbo);
		idraw->get_scene()->update_meshbuilder(editor_mb_handle, mbo);
	}
	void fill_out_struct(MeshBuilder_Object& obj) {
		obj.depth_tested = true;
		obj.owner = this;
		obj.transform = get_ws_transform();
		obj.visible = true;
		obj.meshbuilder = &editor_mb;
	}

	handle<MeshBuilder_Object> editor_mb_handle;
	MeshBuilder editor_mb;
};
CLASS_IMPL(MeshBuilderComponent);

void CapsuleComponent::on_init() {
	if (disable_physics)
		return;
	actor = g_physics.allocate_physics_actor(this);

	auto preset = (physics_preset.ptr) ? (PhysicsFilterPresetBase*)physics_preset.ptr->default_class_object : nullptr;
	actor->init_physics_shape(preset, get_ws_transform(), simulate_physics, send_overlap, send_hit, is_static, is_trigger, disable_physics);
	actor->add_vertical_capsule_to_actor(glm::vec3(0.f), height, radius);
	actor->update_mass();

	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_and_attach_component_type<BillboardComponent>(this);
		b->set_texture(default_asset_load<Texture>("icon/_nearest/capsule_collider.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize
	}
}

void BoxComponent::on_init() {
	actor = g_physics.allocate_physics_actor(this);
	auto preset = (physics_preset.ptr) ? (PhysicsFilterPresetBase*)physics_preset.ptr->default_class_object : nullptr;
	actor->init_physics_shape(preset, get_ws_transform(), simulate_physics, send_overlap, send_hit, is_static, is_trigger, disable_physics);
	actor->add_box_shape_to_actor(get_ws_transform(), side_len);
	actor->update_mass();

	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_and_attach_component_type<BillboardComponent>(this);
		b->set_texture(default_asset_load<Texture>("icon/_nearest/box_collider.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize

		editor_view = get_owner()->create_and_attach_component_type<MeshBuilderComponent>(this);
		auto& mb = editor_view->editor_mb;
		mb.Begin();
		mb.PushLineBox(glm::vec3(-0.5),glm::vec3(0.5), COLOR_RED);
		mb.End();
	}
}


void SphereComponent::on_init() {
	actor = g_physics.allocate_physics_actor(this);
	auto preset = (physics_preset.ptr) ? (PhysicsFilterPresetBase*)physics_preset.ptr->default_class_object : nullptr;
	actor->init_physics_shape(preset, get_ws_transform(), simulate_physics, send_overlap, send_hit, is_static, is_trigger, disable_physics);
	actor->add_sphere_shape_to_actor(get_ws_position(), radius);
	actor->update_mass();

	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_and_attach_component_type<BillboardComponent>(this);
		b->set_texture(default_asset_load<Texture>("icon/_nearest/sphere_collider.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize

		editor_view = get_owner()->create_and_attach_component_type<MeshBuilderComponent>(this);
		auto& mb = editor_view->editor_mb;
		mb.Begin();
		mb.AddSphere({}, radius, 8, 8, COLOR_RED);
		mb.End();
	}
}
void PhysicsComponentBase::on_deinit() {
	if (!actor)
		return;
	g_physics.free_physics_actor(actor);
}
void PhysicsComponentBase::on_changed_transform() {
	if (!actor)
		return;
	assert(actor);
	actor->set_transform(get_ws_transform());
}
PhysicsComponentBase::~PhysicsComponentBase()
{
}
PhysicsComponentBase::PhysicsComponentBase() {}