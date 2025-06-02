#pragma once
#include "Assets/IAsset.h"

CLASS_H(Script, IAsset)
public:
	// contains:
	// compilied lua bytecode
	// exported var names+types 
	// event hook string names

	// attached with ScriptComponent
	// which will hook up events and set exported var values

	bool load_asset(IAssetLoadingInterface*) final;
	void post_load() final {}
	void uninstall() final;
	void sweep_references(IAssetLoadingInterface*) const final {
		sys_print(Debug, "sweep");
	}
	void move_construct(IAsset* src) final;

	std::string script_str;
};