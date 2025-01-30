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

	virtual bool load_asset(ClassBase*& outUserStruct);
	virtual void post_load(ClassBase* inUserStruct) {}
	virtual void uninstall();
	virtual void sweep_references() const {
		sys_print(Debug, "sweep");
	}
	virtual void move_construct(IAsset* src);
	std::string script_str;
};