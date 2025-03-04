#include "LevelAssets.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetRegistry.h"

#include "Framework/Files.h"
#include "Game/BaseUpdater.h"

#include "LevelSerialization/SerializationAPI.h"

#include "Framework/ReflectionProp.h"

CLASS_IMPL(SceneAsset);
CLASS_IMPL(PrefabAsset);

#ifdef EDITOR_BUILD
class IEditorTool;
extern IEditorTool* g_editor_doc;
class MapAssetMetadata : public AssetMetadata
{
public:
	MapAssetMetadata() {
		extensions.push_back("tmap");
		extensions.push_back("bmap");
	}

	// Inherited via AssetMetadata
	virtual Color32 get_browser_color()  const override
	{
		return { 185, 235, 237 };
	}

	virtual std::string get_type_name() const  override
	{
		return "Map";
	}

	virtual bool assets_are_filepaths()  const { return true; }

	virtual IEditorTool* tool_to_edit_me() const { return g_editor_doc; }

	virtual const ClassTypeInfo* get_asset_class_type() const { return &SceneAsset::StaticType; }

	const char* get_arg_for_editortool() const { return "scene"; }

};
static AutoRegisterAsset<MapAssetMetadata> map_register_0987;

class PrefabAssetMetadata : public AssetMetadata
{
public:
	PrefabAssetMetadata() {
		extensions.push_back("pfb");
	}

	// Inherited via AssetMetadata
	virtual Color32 get_browser_color()  const override
	{
		return {255, 117, 133 };
	}

	virtual std::string get_type_name() const  override
	{
		return "Prefab";
	}

	virtual bool assets_are_filepaths()  const { return true; }

	virtual IEditorTool* tool_to_edit_me() const { return g_editor_doc; }

	virtual const ClassTypeInfo* get_asset_class_type() const { return &PrefabAsset::StaticType; }

	const char* get_arg_for_editortool() const { return "prefab"; }
};
static AutoRegisterAsset<PrefabAssetMetadata> prefab_register_0987;
#endif

SceneAsset::~SceneAsset() {
}

void SceneAsset::uninstall() {
	if (sceneFile.get()) {
		sys_print(Warning, "scene asset with non-null scenefile\n");
		for (auto& o : sceneFile->get_objects())
			delete o.second;
		sceneFile.reset(nullptr);
	}
}
void SceneAsset::move_construct(IAsset*) {
	sys_print(Warning, "scene asset move construct, shouldnt have happened\n");
}

bool SceneAsset::load_asset(ClassBase*&)
{
	auto& path = get_name();

	auto fileptr = FileSys::open_read_game(path.c_str());
	if (!fileptr) {
		sys_print(Error, "couldn't open scene %s\n", path.c_str());
		return false;
	}
	text = std::string(fileptr->size(), ' ');
	fileptr->read((void*)text.data(), text.size());
	try {
		sceneFile = std::make_unique<UnserializedSceneFile>(unserialize_entities_from_text(text));
	}
	catch (int) {
		sys_print(Error, "error loading SceneAsset %s\n", path.c_str());
		return false;
	}

	return true;
}

PrefabAsset::~PrefabAsset() {
}

bool PrefabAsset::load_asset(ClassBase*&)
{
	auto& path = get_name();

	auto fileptr = FileSys::open_read_game(path.c_str());
	if (!fileptr) {
		sys_print(Error, "couldn't open scene %s\n", path.c_str());
		return false;
	}
	text = std::string(fileptr->size(), ' ');
	fileptr->read((void*)text.data(), text.size());
	try {
		sceneFile = std::make_unique<UnserializedSceneFile>(unserialize_entities_from_text(text, this));
		// add instance ids here for diff'ing entity references
		uint64_t id = 1ull << 63ull;
		for (auto obj : sceneFile->get_objects()) {
			obj.second->post_unserialization(++id);
			instance_ids_for_diffing.insert(id, obj.second);
		}
		sceneFile->unserialize_post_assign_ids();
	}
	catch (int) {
		sys_print(Error, "error loading PrefabAsset %s\n", path.c_str());
		return false;
	}

	return true;
}
void PrefabAsset::uninstall()
{
	sys_print(Debug, "prefab uninstalled %s\n", get_name().c_str());
	for (auto& o : sceneFile->get_objects())
		delete o.second;
	sceneFile.reset(nullptr);
}


static void check_props_for_assetptr(void* inst, const PropertyInfoList* list)
{
	for (int i = 0; i < list->count; i++) {
		auto prop = list->list[i];
		if (strcmp(prop.custom_type_str, "AssetPtr") == 0) {
			IAsset** e = (IAsset**)prop.get_ptr(inst);
			if (*e)
				g_assets.touch_asset(*e);
		}
		else if(prop.type==core_type_id::List) {
			auto listptr = prop.get_ptr(inst);
			auto size = prop.list_ptr->get_size(listptr);
			for (int j = 0; j < size; j++) {
				auto ptr = prop.list_ptr->get_index(listptr, j);
				check_props_for_assetptr(ptr, prop.list_ptr->props_in_list);
			}
		}
	}
}

void PrefabAsset::sweep_references() const
{
	sys_print(Debug, "prefab sweep ref %s\n", get_name().c_str());
	{
		for (auto& obj : sceneFile->get_objects()) {
			auto o = obj.second;
			auto type = &o->get_type();
			while (type) {
				auto props = type->props;
				if(props)
					check_props_for_assetptr(o, props);
				type = type->super_typeinfo;
			}
		}
	}
}
void PrefabAsset::move_construct(IAsset* other)
{
	sys_print(Debug, "prefab move construct %s\n", get_name().c_str());
	if (sceneFile.get()) {
		PrefabAsset::uninstall();
	}
	ASSERT(!sceneFile.get());
	PrefabAsset* o = (PrefabAsset*)other;
	sceneFile = std::move(o->sceneFile);
	text = std::move(o->text);
	instance_ids_for_diffing = std::move(o->instance_ids_for_diffing);
}
