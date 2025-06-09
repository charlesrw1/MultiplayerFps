#pragma once
#include "Framework/SerializerJson.h"
class PrefabAsset;
class UnserializedSceneFile;
class IAssetLoadingInterface;
class MakePathForObjectNew : public IMakePathForObject
{
public:
	MakePathForObjectNew(PrefabAsset* opt_prefab);
	// Inherited via IMakePathForObject
	std::string make_path(const ClassBase* to) override;
	std::string make_type_name(ClassBase* obj) override;
	nlohmann::json* find_diff_for_obj(ClassBase* obj) override;
	PrefabAsset* for_prefab = nullptr;
};
class MakeObjectForPathNew : public IMakeObjectFromPath {
public:
	MakeObjectForPathNew(IAssetLoadingInterface& load, UnserializedSceneFile& out, PrefabAsset* for_prefab);
	ClassBase* create_from_name(Serializer& s, const std::string& str) override;
	PrefabAsset* prefab = nullptr;
	UnserializedSceneFile& out;
	IAssetLoadingInterface& load;
};