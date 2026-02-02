#include "SpawnerComponenth.h"

#include "../LevelAssets.h"

SpawnerComponent::~SpawnerComponent()
{
}

SpawnerComponent::SpawnerComponent()
{
	set_call_init_in_editor(true);
}
#include "MeshComponent.h"
void SpawnerComponent::start()
{
	if (eng->is_editor_level()) {
		set_model();
	}

}

void SpawnerComponent::stop()
{
}

void SpawnerComponent::serialize(Serializer& s)
{
	if (s.is_saving()) {
		if (obj["_type"].is_string()) {
			string str = obj["_type"];
			s.serialize("_type", str);
		}
	}
	else {
		string str;
		s.serialize("_type",str);
		obj["_type"] = str;
	}
	// finish fixme
}

void SpawnerComponent::set(string schema_name)
{
	obj = nlohmann::json();
	obj = SchemaManager::get().schema_file[schema_name];
	obj["_type"] = schema_name;
	set_model();
}
#include "BillboardComponent.h"
void SpawnerComponent::set_model()
{
	string model_str = "";
	if (obj["_type"].is_string()) {
		string  s = obj["_type"];
		auto& schema = SchemaManager::get().schema_file[s];
		if (schema.is_object() && schema["_model"].is_string()) {
			model_str = schema["_model"];
		}
		auto& ms = obj["model"];
		if (ms.is_string() && !std::string(ms).empty()) {
			model_str = ms;
		}
	}
	

	auto mesh = get_owner()->get_component<MeshComponent>();
	if (mesh) mesh->destroy();
	auto bb = get_owner()->get_component<BillboardComponent>();
	if (bb)
		bb->destroy();


	if (model_str.empty()) {
		bb = get_owner()->create_component<BillboardComponent>();
		bb->set_texture(Texture::load("dude.png"));
	}
	else {
		mesh = get_owner()->create_component<MeshComponent>();
		mesh->set_model_str(model_str.c_str());
		mesh->set_editor_transient(true);
	}

}
