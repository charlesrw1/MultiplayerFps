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
	string get_spawner_type() {
		return obj["_type"];
	}
private:
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