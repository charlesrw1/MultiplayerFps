#pragma once

#include "../Entity.h"
#include "../EntityComponent.h"
#include <json.hpp>
class SpawnerComponent : public Component
{
public:
	CLASS_BODY(SpawnerComponent);

	~SpawnerComponent();
	SpawnerComponent();

	void start() final;
	void stop() final;
	void serialize(Serializer&) final;
	void set(string schema_name);
	nlohmann::json obj;
	void set_model();

	REF string get_spawner_type();
	REF bool has_key(string name) {
		return !obj[name].is_null();
	}
	REF float get_float(string name) {
		return float(std::atof(string(obj[name]).c_str()));
	}
	REF int get_int(string name) {
		return int(std::atoi(string(obj[name]).c_str()));
	}
	REF string get_string(string name) {
		return string(obj[name]);
	}
};

class FoliageContainerComponent : public Component {
public:
	void serialize(Serializer&) final;
	void start() final;
	void stop() final;
	
	struct FoliageItem {
		glm::mat4x3 transform{};
		int model_index = 0;
	};
	std::vector<Model*> foliage_models;
	std::vector<FoliageItem> transforms;
	std::vector<handle<Render_Object>> objects;
};