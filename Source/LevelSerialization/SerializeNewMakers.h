#pragma once
#include "Framework/SerializerJson.h"
class PrefabAsset;
class UnserializedSceneFile;
class IAssetLoadingInterface;
class ReadSerializerBackendJson;
class MakePathForObjectNew : public IMakePathForObject
{
public:
	MakePathForObjectNew();
	// Inherited via IMakePathForObject
	MakePath make_path(const ClassBase* to) override;
	std::string make_type_name(ClassBase* obj) override;
	nlohmann::json* find_diff_for_obj(ClassBase* obj) override;
};
class MakeObjectForPathNew : public IMakeObjectFromPath {
public:
	MakeObjectForPathNew(IAssetLoadingInterface& load, UnserializedSceneFile& out);
	ClassBase* create_from_name(ReadSerializerBackendJson& s, const std::string& str, const string& parent_path) override;
	UnserializedSceneFile& out;
	IAssetLoadingInterface& load;
};