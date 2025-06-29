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
	for (auto& o : all_objs)
		delete o.second;
	all_objs.clear();
}

BaseUpdater* UnserializedSceneFile::find(const std::string& path)
{
	auto find = all_objs.find(path);
	return find == all_objs.end() ? nullptr : find->second;
}

void UnserializedSceneFile::add_components_and_children_from_entity_R(const std::string& path, Entity* e, Entity* source)
{
	for (auto& c : e->all_components)
	{
		auto cpath = path + "~" + std::to_string(c->unique_file_id);
		all_objs.insert({ cpath,c });
	}
	for (auto& child : e->get_children())
	{
		auto cpath = path + "~" + std::to_string(child->unique_file_id);
		all_objs.insert({ cpath, child });
		add_components_and_children_from_entity_R(cpath+"/", child, source);
	}
}

uint32_t parse_fileid(const std::string& path)
{
	auto last_slash = path.rfind('/');
	if (last_slash != std::string::npos) {
		if (path.size() == 1) return 0;

		ASSERT(last_slash != path.size() - 1);
		ASSERT(path.at(last_slash + 1) != '~');
		return std::stoll(path.substr(last_slash + 1));
	}
	else
		return std::stoll(path);
}

#include "SerializeNew.h"


#include "SerializeNew.h"
#include "Framework/StringUtils.h"

UnserializedSceneFile unserialize_entities_from_text(const char* debug_tag, const std::string& text, IAssetLoadingInterface* load)
{
	if (!load)
		load = AssetDatabase::loader;
	if (StringUtils::starts_with(text, "!json\n")) {
		auto fixedtext = text.substr(5);
		return NewSerialization::unserialize_from_text(debug_tag,fixedtext, *load);
	}
	else {
		sys_print(Error, "unserialize_entities_from_text: old format not supported\n");
		return UnserializedSceneFile();
	}
}

void check_props_for_entityptr(void* inst, const PropertyInfoList* list)
{
	for (int i = 0; i < list->count; i++) {
		auto prop = list->list[i];
		if (prop.type==core_type_id::ObjHandlePtr) {
			// wtf!
			BaseUpdater** e = (BaseUpdater**)prop.get_ptr(inst);

			obj<BaseUpdater>* eptr = (obj<BaseUpdater>*)prop.get_ptr(inst);
			if (*e) {
				*eptr = obj<BaseUpdater>((*e)->get_instance_id());
			}
		}
		else if(prop.type==core_type_id::List) {
			auto listptr = prop.get_ptr(inst);
			auto size = prop.list_ptr->get_size(listptr);
			for (int j = 0; j < size; j++) {
				auto ptr = prop.list_ptr->get_index(listptr, j);
				check_props_for_entityptr(ptr, prop.list_ptr->props_in_list);
			}
		}
	}
}

void UnserializedSceneFile::unserialize_post_assign_ids()
{
	for (auto& obj : all_objs) {
		if (!obj.second)
			continue;
		auto type = &obj.second->get_type();
		while (type) {
			auto props = type->props;
			if(props)
				check_props_for_entityptr(obj.second, props);
			type = type->super_typeinfo;
		}
	}
}