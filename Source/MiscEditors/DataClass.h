#pragma once

// A data class is simply a ClassBase object that can be serialized and edited
// Allows for data driven assets in a really easy way!!!
// Just create a ClassBase inherited object, then open the editor with the classes name ("start_ed DataClass <ClassName>")
// This has the advantage of not needing manual parsers and it integrates 
// with all custom property editors! (like assets or other ClassBase's, just drag and drop, omg I love this so much)

#include "Assets/IAsset.h"
#include "Assets/AssetLoaderRegistry.h"

CLASS_H(DataClass, IAsset)
public:
	const ClassBase* get_obj() const {
		return object;
	}

private:
	ClassBase* object = nullptr;

	friend class DataClassLoader;
};

class DataClassLoader : public IAssetLoader
{
public:
	const DataClass* load_dataclass_no_check(const std::string& file);

	IAsset* load_asset(const std::string& path) override {
		return (DataClass*)load_dataclass_no_check(path);	// fixme, const
	}

	// loads the data class with a type check
	template<typename T>
	const DataClass* load_dataclass(const std::string& file) {
		auto dc = load_dataclass_no_check(file);
		if (!dc) return nullptr;
		return dc->object->cast_to<T>();	// dynamic cast check
	}

private:
	std::unordered_map<std::string, DataClass*> all_dataclasses;
};

extern DataClassLoader g_dc_loader;