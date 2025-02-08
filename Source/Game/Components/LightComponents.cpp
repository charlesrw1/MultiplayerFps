
#include "LightComponents.h"
#include "Render/Render_Light.h"
#include "Render/Render_Sun.h"
#include "Render/DrawPublic.h"
#include "Render/Texture.h"

#include "BillboardComponent.h"
#include "ArrowComponent.h"

#include "Game/Entity.h"
#include "Assets/AssetDatabase.h"

#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"

#include "GameEnginePublic.h"

#include "Game/AssetPtrMacro.h"


void SpotLightComponent::build_render_light(Render_Light& light)
{
	light.color = glm::vec3(color.r, color.g, color.b) * (intensity / 255.f);
	light.projected_texture = cookie_asset.get();
	light.conemax = cone_angle;
	light.conemin = inner_cone;
	light.radius = radius;
	light.is_spotlight = true;

	auto& transform = get_owner()->get_ws_transform();

	light.position = transform[3];
	light.normal = glm::normalize(-transform[2]);
}

void SpotLightComponent::start()
{
	Render_Light light;
	build_render_light(light);
	light_handle = idraw->get_scene()->register_light(light);

	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(default_asset_load<Texture>("icon/_nearest/flashlight.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize

		auto arrow_obj = get_owner()->create_child_entity<Entity>();
		arrow_obj->dont_serialize_or_edit = true;
		arrow_obj->create_component<ArrowComponent>();
		arrow_obj->set_ls_transform(glm::vec3(0,0,0.4), {}, glm::vec3(0.25f));
		editor_arrow = arrow_obj->get_instance_id();
		editor_billboard = b->get_instance_id();
	}
}

void SpotLightComponent::editor_on_change_property()
{
	assert(light_handle.is_valid());
	Render_Light light;
	build_render_light(light);
	idraw->get_scene()->update_light(light_handle, light);
}

void SpotLightComponent::end()
{
	idraw->get_scene()->remove_light(light_handle);
	auto e = eng->get_object(editor_billboard);
	if (e)
		((EntityComponent*)e)->destroy();
	e = eng->get_object(editor_arrow);
	if (e)
		((EntityComponent*)e)->destroy();
}

void SpotLightComponent::on_changed_transform()
{
	assert(light_handle.is_valid());
	Render_Light light;
	build_render_light(light);
	idraw->get_scene()->update_light(light_handle, light);
}


void PointLightComponent::build_render_light(Render_Light& light)
{
	light.color = glm::vec3(color.r, color.g, color.b) * (intensity / 255.f);
	light.radius = radius;
	light.is_spotlight = false;

	auto& transform = get_owner()->get_ws_transform();

	light.position = transform[3];
}

void PointLightComponent::start()
{
	Render_Light light;
	build_render_light(light);
	light_handle = idraw->get_scene()->register_light(light);

	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(default_asset_load<Texture>("icon/pointBig.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize
		editor_billboard = b->get_instance_id();
	}
}

void PointLightComponent::end()
{
	idraw->get_scene()->remove_light(light_handle);
	auto e = eng->get_object(editor_billboard);
	if (e)
		((EntityComponent*)e)->destroy();
}

void PointLightComponent::on_changed_transform()
{
	assert(light_handle.is_valid());
	Render_Light light;
	build_render_light(light);
	idraw->get_scene()->update_light(light_handle, light);
}

void PointLightComponent::editor_on_change_property()
{
	assert(light_handle.is_valid());
	Render_Light light;
	build_render_light(light);
	idraw->get_scene()->update_light(light_handle, light);
}

void SunLightComponent::editor_on_change_property()
{
	assert(light_handle.is_valid());
	Render_Sun light;
	build_render_light(light);
	idraw->get_scene()->update_sun(light_handle, light);
}

void SunLightComponent::build_render_light(Render_Sun& light)
{
	light.color = glm::vec3(color.r,color.g,color.b)*(intensity/255.f);
	light.fit_to_scene = fit_to_scene;
	light.log_lin_lerp_factor = log_lin_lerp_factor;
	light.z_dist_scaling = z_dist_scaling;
	light.max_shadow_dist = max_shadow_dist;
	light.epsilon = epsilon;
	light.cast_shadows = true;

	auto& transform = get_owner()->get_ws_transform();
	light.direction = glm::normalize(-transform[2]);
}

void SunLightComponent::start()
{
	Render_Sun light;
	build_render_light(light);
	light_handle = idraw->get_scene()->register_sun(light);

	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(default_asset_load<Texture>("icon/_nearest/sun.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize

		auto s = get_owner()->create_component<ArrowComponent>();
		s->dont_serialize_or_edit = true;
		editor_arrow = s->get_instance_id();
		editor_billboard = b->get_instance_id();
	}
}

void SunLightComponent::end()
{
	idraw->get_scene()->remove_sun(light_handle);

	auto e = eng->get_object(editor_billboard);
	if (e)
		((EntityComponent*)e)->destroy();
	e = eng->get_object(editor_arrow);
	if (e)
		((EntityComponent*)e)->destroy();
}

void SunLightComponent::on_changed_transform()
{
	assert(light_handle.is_valid());
	Render_Sun light;
	build_render_light(light);
	idraw->get_scene()->update_sun(light_handle, light);
}

SunLightComponent::~SunLightComponent() {}
PointLightComponent::~PointLightComponent() {}
SpotLightComponent::~SpotLightComponent() {}
#include "Render/Render_Volumes.h"

void SkylightComponent::start() {

	mytexture = new Texture; // g_imgs.install_system_texture("_skylight");
	Render_Skylight sl;
	sl.generated_cube = mytexture;
	sl.wants_update = true;

	handle = idraw->get_scene()->register_skylight(sl);
}
void SkylightComponent::end() {
	idraw->get_scene()->remove_skylight(handle);
	delete mytexture;
	mytexture = nullptr;
}

void SkylightComponent::editor_on_change_property()  {
	if (recapture_skylight) {
		sys_print(Debug, "recapturing skylight");
		Render_Skylight sl;
		sl.generated_cube = mytexture;
		sl.wants_update = true;
		idraw->get_scene()->update_skylight(handle, sl);
	}
	recapture_skylight = false;
}

#include "Game/Entity.h"
CLASS_H(SkylightEntity, Entity)
public:
	SkylightEntity() {
		Skylight = construct_sub_component<SkylightComponent>("Skylight");

		if (eng->is_editor_level()) {
			auto b = construct_sub_component<BillboardComponent>("Billboard");
			b->set_texture(default_asset_load<Texture>("icon/_nearest/skylight.png"));
			b->dont_serialize_or_edit = true;	// editor only item, dont serialize
		}
	}
	SkylightComponent* Skylight = nullptr;

	static const PropertyInfoList* get_props() = delete;
};
CLASS_IMPL(SkylightEntity);

SpotLightComponent::SpotLightComponent() {
	set_call_init_in_editor(true);
}
PointLightComponent::PointLightComponent() {
	set_call_init_in_editor(true);
}

SunLightComponent::SunLightComponent() {
	set_call_init_in_editor(true);
}
#include "Game/Components/MeshbuilderComponent.h"
void CubemapComponent::start() {
	mytexture = new Texture;
	Render_Reflection_Volume vol;
	fill_out_struct(vol);
	handle = idraw->get_scene()->register_reflection_volume(vol);

	if (eng->is_editor_level()) {
		editor_meshbuilder = get_owner()->create_component<MeshBuilderComponent>();
		editor_meshbuilder->editor_transient = true;
		editor_meshbuilder->use_background_color = true;
		editor_meshbuilder->use_transform = false;
		update_editormeshbuilder();
	}
}
void CubemapComponent::update_editormeshbuilder()
{
	if (!editor_meshbuilder)
		return;
	glm::vec3 scale = get_owner()->get_ls_scale();
	auto boxmin = get_ws_position() - scale * 0.5f;
	auto boxmax = get_ws_position() + scale * 0.5f;
	editor_meshbuilder->mb.Begin();
	editor_meshbuilder->mb.PushLineBox(boxmin, boxmax, COLOR_GREEN);
	editor_meshbuilder->mb.End();
}
void CubemapComponent::end() {
	idraw->get_scene()->remove_reflection_volume(handle);
	delete mytexture;
	mytexture = nullptr;
	if (editor_meshbuilder) {
		editor_meshbuilder->destroy();
		editor_meshbuilder = nullptr;
	}
}
void CubemapComponent::editor_on_change_property() {
	if (recapture) {
		recapture = false;
		Render_Reflection_Volume vol;
		fill_out_struct(vol);
		vol.wants_update = true;
		idraw->get_scene()->update_reflection_volume(handle, vol);
	}
	update_editormeshbuilder();
}
void CubemapComponent::fill_out_struct(Render_Reflection_Volume& h)
{
	h.generated_cube = mytexture;
	h.probe_position = get_ws_transform() * glm::vec4(anchor.p, 1.0);
	glm::vec3 scale = get_owner()->get_ls_scale();
	h.boxmin = get_ws_position() - scale*0.5f;
	h.boxmax = get_ws_position() + scale * 0.5f;
}

#ifdef EDITOR_BUILD
// FIXME!
#include "LevelEditor/EditorDocLocal.h"
#include "Framework/AddClassToFactory.h"
class CubemapAnchorEditor : public IPropertyEditor
{
public:
	~CubemapAnchorEditor() {
		if (ed_doc.manipulate->is_using_key_for_custom(this))
			ed_doc.manipulate->stop_using_custom();
	}
	// Inherited via IPropertyEditor
	virtual bool internal_update() override
	{
		CubemapAnchor* j = (CubemapAnchor*)prop->get_ptr(instance);
		Entity* me = ed_doc.selection_state->get_only_one_selected().get();
		if (!me) {
			ImGui::Text("no Entity* found\n");
			return false;
		}

		ImGui::Checkbox("edit_anchor", &using_this);

		if (!using_this) {
			ed_doc.manipulate->stop_using_custom();
		}

		if(using_this) {
			if (ed_doc.manipulate->is_using_key_for_custom(this)) {
				auto last_matrix = ed_doc.manipulate->get_custom_transform();
				auto local = glm::inverse(me->get_ws_transform()) * last_matrix;
				j->p = local[3];
			};
		}
		
		bool ret = false;
		if (ImGui::DragFloat3("##vec", (float*)&j->p, 0.05))
			ret = true;
		
		if(using_this) {
			glm::mat4 matrix = glm::translate(glm::mat4(1.f), j->p);
			ed_doc.manipulate->set_start_using_custom(this, me->get_ws_transform() * matrix);
		
			return true;
		}
		
		return ret;

	}
	bool using_this = false;
};

ADDTOFACTORYMACRO_NAME(CubemapAnchorEditor, IPropertyEditor, "CubemapAnchor");
#endif
class CubemapAnchorSerializer : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		const CubemapAnchor* j = (const CubemapAnchor*)info.get_ptr(inst);
		return string_format("%f %f %f %d", j->p.x, j->p.y, j->p.z,(int)j->worldspace);
	}
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user) override
	{
		CubemapAnchor* j = (CubemapAnchor*)info.get_ptr(inst);
		std::string to_str(token.str_start, token.str_len);
		int d = 0;
		int args = sscanf(to_str.c_str(), "%f %f %f %d", &j->p.x, &j->p.y, &j->p.z,&d);
		j->worldspace = bool(d);
		if (args != 4) 
			sys_print(Warning, "Anchor joint unserializer fail\n");
	}
};
ADDTOFACTORYMACRO_NAME(CubemapAnchorSerializer, IPropertySerializer, "CubemapAnchor");