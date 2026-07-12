#include "EntityComponent.h"
#include "glm/gtx/euler_angles.hpp"
#include "Entity.h"
#include "Level.h"
#include "Assets/AssetRegistry.h"
#include "GameEnginePublic.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/Serializer.h"
#include "Scripting/ScriptManager.h"
#include <stdexcept>
#include "Components/MeshComponent.h"

#ifdef EDITOR_BUILD
#include "imgui.h"
extern void OpenInVSCode(const std::string& full_path, int line);

// create native entities as a fake "Asset" for drag+drop and double click open to create instance abilities
class ComponentTypeMetadata : public AssetMetadata
{
public:
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const override { return {181, 159, 245}; }

	virtual std::string get_type_name() const override { return "Component-Entity"; }

	// gamepath here is actually the component's classname (see fill_extra_assets below);
	// this pseudo-asset type has no real file of its own. For Lua-defined components,
	// jump to the .lua file that defines them instead.
	void draw_browser_context_menu(const string& gamepath) final {
		auto* ti = ClassBase::find_class(gamepath.c_str());
		if (!ti || !ti->get_is_lua_class())
			return;
		auto* lua_ti = static_cast<const LuaClassTypeInfo*>(ti);
		if (lua_ti->get_source_file().empty() || lua_ti->get_source_line() <= 0)
			return;
		if (ImGui::MenuItem("Open in code editor"))
			OpenInVSCode(lua_ti->get_source_file(), lua_ti->get_source_line());
	}

	virtual void fill_extra_assets(std::vector<std::string>& filepaths) const override {

		// all editor spawnable components here fixme
		filepaths.push_back("DecalComponent");

		filepaths.push_back("SpotLightComponent");
		filepaths.push_back("PointLightComponent");
		filepaths.push_back("SunLightComponent");
		filepaths.push_back("SkylightComponent");
		filepaths.push_back("CubemapComponent");
		filepaths.push_back("LightmapComponent");

		filepaths.push_back("MeshComponent");

		filepaths.push_back("ParticleComponent");
		filepaths.push_back("TrailComponent");
		filepaths.push_back("BeamComponent");

		filepaths.push_back("SoundComponent");

		filepaths.push_back("ParticleSystemComponent");

		filepaths.push_back("AnimPreviewComponent");

		filepaths.push_back("WorldTextComponent");
		filepaths.push_back("GiVolumeComponent");

		filepaths.push_back("PostProcessComponent");

		filepaths.push_back("AreaishLightComponent");

		filepaths.push_back("NavMeshVolumeComponent");
		filepaths.push_back("NavMeshSettingsComponent");
		filepaths.push_back("NavAgentComponent");

		// Surface only Lua-defined Component subclasses whose annotation block
		// contains a `---editor` tag (see ScriptLoadingUtil::parse_text).
		// Untagged Lua components remain fully usable from script but stay out
		// of the editor picker to keep authoring-only types from cluttering it.
		for (auto it = ClassBase::get_subclasses(&Component::StaticType); !it.is_end(); it.next()) {
			auto* ti = it.get_type();
			if (ti && ti->editor_spawnable)
				filepaths.push_back(ti->get_classname());

			if (!ti || !ti->get_is_lua_class() || !ti->has_allocate_func())
				continue;
			auto* lua_ti = static_cast<const LuaClassTypeInfo*>(ti);
			if (lua_ti->is_editor_placeable())
				filepaths.push_back(ti->classname);
		}
	}
	virtual const ClassTypeInfo* get_asset_class_type() const { return &Component::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(ComponentTypeMetadata);
#endif

void Component::set_owner_dont_serialize_or_edit(bool b) {
	if (get_owner())
		get_owner()->dont_serialize_or_edit = b;
}
void Component::set_ticking(bool shouldTick) {
	auto level = eng->get_level();
	if (level && tick_enabled != shouldTick && init_state == initialization_state::CALLED_START) {
		if (shouldTick)
			level->add_to_update_list(this);
		else
			level->remove_from_update_list(this);
	}
	tick_enabled = shouldTick;
}
#include "Framework/Log.h"
void Component::serialize(Serializer& s) {}
void Component::init_updater() {
	auto level = eng->get_level();
	ASSERT(level);
	ASSERT(init_state == initialization_state::HAS_ID);
	if (level && tick_enabled)
		level->add_to_update_list(this);
}
void Component::shutdown_updater() {
	auto level = eng->get_level();
	ASSERT(level);
	if (level && tick_enabled)
		level->remove_from_update_list(this);
}

void Component::ensure_lua_shadow() {
	ScriptManager::ensure_shadow_for(this);
}

void Component::activate_internal_step2() {
	ASSERT(init_state == initialization_state::HAS_ID);
	ASSERT(entity_owner);
	if (eng->is_editor_level())
		editor_start();
	if (!eng->is_editor_level() || get_call_init_in_editor()) {
		// Only bake shadow values into the raw Lua table outside the editor: it's a one-time
		// lua_rawset (see sync_shadow_to_lua_table) that permanently defeats __index/__newindex
		// for those keys (Lua only invokes metamethods when the key is absent). For an
		// init_in_editor component, start() runs while still in the editor, so baking here
		// would desync the property grid (shadow) from Lua (table) for the rest of its life.
		if (!eng->is_editor_level())
			ScriptManager::sync_shadow_to_lua_table(this);
		start();
		init_updater();
		init_state = initialization_state::CALLED_START;
	}
}

void Component::deactivate_internal() {
	// can call stop after a pre_start()
	ASSERT(init_state == initialization_state::CALLED_START);
	if (!eng->is_editor_level() || get_call_init_in_editor()) {
		stop();
	}
	shutdown_updater();
	init_state = initialization_state::HAS_ID;
}

void Component::destroy() {
	ASSERT(eng->get_level());
	eng->get_level()->destroy_component(this);
}
void Component::sync_render_data() {
	// changed to start or pre_start
	// if(init_state == initialization_state::CALLED_START)
	eng->get_level()->add_to_sync_render_data_list(this);
}

void Component::destroy_internal() {
	if (init_state == initialization_state::CALLED_START)
		deactivate_internal();
	ASSERT(entity_owner);
	entity_owner->remove_this_component_internal(this);
	ASSERT(entity_owner == nullptr);
}

void Component::editor_set_model(std::string_view modelname, bool draw_text) {
	if (eng->is_editor_app()) {
		auto* mesh = entity_owner->get_component<MeshComponent>();
		if (!mesh)
			mesh = entity_owner->create_component<MeshComponent>();
		mesh->set_model_str(modelname.data());
		mesh->dont_serialize_or_edit=true;
		this->draw_text_in_editor = draw_text;
	}
}
#include <Game/Components/BillboardComponent.h>
void Component::editor_set_billboard(std::string_view billboard_texture, bool draw_text, float scale) {
	if (eng->is_editor_app()) {
		auto* mesh = entity_owner->get_component<BillboardComponent>();
		if (!mesh)
			mesh = entity_owner->create_component<BillboardComponent>();
		mesh->set_texture(Texture::load(billboard_texture.data()));
		mesh->dont_serialize_or_edit = true;
		this->draw_text_in_editor = draw_text;
	}
}

Component::~Component() {
	ASSERT(init_state != initialization_state::CALLED_START);
	// Drop ourselves from any Lua live-instance set and run destructors over
	// non-POD PROP_LUA_BACKED fields (e.g. std::string) before lua_field_shadow's
	// unique_ptr releases the raw byte buffer.
	ScriptManager::on_component_destructed(this);
}

const glm::mat4& Component::get_ws_transform() {
	return get_owner()->get_ws_transform();
}