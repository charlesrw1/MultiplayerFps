#include "SerializationAPI.h"
#include "Framework/DictParser.h"
#include "Framework/ClassBase.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "AssetCompile/Someutils.h"
#include <stdexcept>
#include "Level.h"
#include "Assets/AssetDatabase.h"
#include "Game/LevelAssets.h"
#include "Framework/ReflectionProp.h"

class UnserializationWrapper;
Entity* unserialize_entities_from_text_internal(UnserializedSceneFile& scene, const std::string& text, const std::string& rootpath, 
	PrefabAsset* prefab, Entity* starting_root, IAssetLoadingInterface* load);


void UnserializedSceneFile::delete_objs()
{
	for (auto& o : all_obj_vec)
		delete o;
	all_obj_vec.clear();
}



#include "SerializeNew.h"


#include "SerializeNew.h"
#include "Framework/StringUtils.h"

UnserializedSceneFile unserialize_entities_from_text(const char* debug_tag, const std::string& text, IAssetLoadingInterface* load, bool keepid)
{
	if (!load)
		load = AssetDatabase::loader;
	if (StringUtils::starts_with(text, "!json\n")) {
		auto fixedtext = text.substr(5);
		return NewSerialization::unserialize_from_text(debug_tag,fixedtext, *load, keepid);
	}
	else {
		sys_print(Error, "unserialize_entities_from_text: old format not supported\n");
		return UnserializedSceneFile();
	}
}

