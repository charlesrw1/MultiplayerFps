#pragma once
#include "Assets/IAsset.h"
#include <vector>

// Map: generic container for entities
// Level map: a singleton map with WorldSettings
// Prefab map: a map that can be instanced

class Entity;
CLASS_H(Map,IAsset)
public:
	// takes the vector that was loaded with the asset
	// used for singleton Map instances like the main Level
	std::vector<Entity*>& steal_default_creation();
	// creates an instance using the stringForm
	// used by prefabs or schemas which are instanced many times
	std::vector<Entity*> create_instance();

	void move_construct(IAsset* o) override;
	bool load_asset(ClassBase*& user) override;
	void post_load(ClassBase*) override;
	void uninstall() override;
	void sweep_references() const override;
protected:

private:
	std::string stringForm;
	std::vector<Entity*> defaultCreation;
};

CLASS_H(Prefab,Map)
public:
};