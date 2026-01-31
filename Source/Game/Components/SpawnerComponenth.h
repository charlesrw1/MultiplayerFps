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
private:
};