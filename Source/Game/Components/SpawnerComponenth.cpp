#include "SpawnerComponenth.h"

#include "../LevelAssets.h"

SpawnerComponent::~SpawnerComponent() {}

SpawnerComponent::SpawnerComponent() {
	set_call_init_in_editor(true);
}
#include "MeshComponent.h"
void SpawnerComponent::start() {
	if (eng->is_editor_level()) {
		set_model();
	}
}

void SpawnerComponent::stop() {}
string SpawnerComponent::get_spawner_type() {
	if (obj.is_null())
		return "__none";
	auto& t = obj["_type"];
	return t.is_string() ? t : "__none";
}

void SpawnerComponent::serialize(Serializer& s) {
	if (s.is_saving()) {
		if (obj["_type"].is_string()) {
			for (auto& [key, value] : obj.items()) {
				if (!value.is_string()) {
					continue;
				}

				string str = value;
				s.serialize(key.c_str(), str);
			}
		}
	} else {
		string str;
		s.serialize("_type", str);
		obj["_type"] = str;

		auto& schemaItem = SchemaManager::get().schema_file[str];
		for (auto& [key, value] : schemaItem.items()) {
			if (key.at(0) != '_') {
				string out;
				if (s.serialize(key.c_str(), out))
					obj[key] = out;
				else
					obj[key] = value;
			}
		}
	}
	// finish fixme
}

void SpawnerComponent::set(string schema_name) {
	obj = nlohmann::json();
	auto& schemaItem = SchemaManager::get().schema_file[schema_name];
	obj["_type"] = schema_name;
	for (auto& [key, value] : schemaItem.items()) {
		if (key.at(0) != '_') {
			obj[key] = value;
		}
	}

	set_model();
}
#include "Render/MaterialPublic.h"
#include "BillboardComponent.h"
void SpawnerComponent::set_model() {
	if (!obj.is_object())
		return;
	string model_str = "";
	MaterialInstance* mat_override{};
	if (obj["_type"].is_string()) {
		string s = obj["_type"];
		auto& schema = SchemaManager::get().schema_file[s];
		if (schema.is_object() && schema["_model"].is_string()) {
			model_str = schema["_model"];
		}
		if (schema.is_object() && schema["_material"].is_string())
			mat_override = MaterialInstance::load(schema["_material"]);
		auto& ms = obj["model"];
		if (ms.is_string() && !std::string(ms).empty()) {
			model_str = ms;
		}
	}

	auto mesh = get_owner()->get_component<MeshComponent>();
	if (mesh)
		mesh->destroy();
	auto bb = get_owner()->get_component<BillboardComponent>();
	if (bb)
		bb->destroy();

	if (model_str.empty()) {
		bb = get_owner()->create_component<BillboardComponent>();
		bb->set_texture(Texture::load("dude.png"));
	} else {
		mesh = get_owner()->create_component<MeshComponent>();
		mesh->set_model_str(model_str.c_str());
		mesh->set_editor_transient(true);
		// dont want these rendering in cubemaps or baking
		mesh->set_ignore_baking(true);
		mesh->set_ignore_cubemap_view(true);
		if (mat_override)
			mesh->set_material_override(mat_override);
	}
}

void FoliageContainerComponent::serialize(Serializer& s) {
#if 0
	if (s.is_loading()) {
		std::string outData;
		if (s.serialize(baked_tag, outData)) {
			auto data = StringUtils::base64_decode(outData);
			if ((data.size() % sizeof(glm::vec3)) != 0) {
				sys_print(Error, "LightmapComponent::serialize: unserialized bin data bad size?\n");
			}
			else {
				const int vec3Count = data.size() / sizeof(glm::vec3);
				bakedProbes.clear();
				bakedProbes.resize(vec3Count);
				for (int i = 0; i < vec3Count; i++) {
					bakedProbes.at(i) = *(glm::vec3*)(&data.at(i * sizeof(glm::vec3)));
				}
			}
		}
	}
	else {
		std::vector<uint8_t> outData;
		outData.resize(bakedProbes.size() * sizeof(glm::vec3));
		for (int i = 0; i < bakedProbes.size(); i++) {
			glm::vec3* outPtr = (glm::vec3*)&outData.at(i * sizeof(glm::vec3));
			*outPtr = bakedProbes.at(i);
		}
		std::string encoded = StringUtils::base64_encode(outData);
		s.serialize(baked_tag, encoded);
	}
#endif
}

void FoliageContainerComponent::start() {}

void FoliageContainerComponent::stop() {}
