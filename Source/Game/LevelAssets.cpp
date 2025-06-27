#include "LevelAssets.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetRegistry.h"

#include "Framework/Files.h"
#include "Game/BaseUpdater.h"

#include "LevelSerialization/SerializationAPI.h"

#include "Framework/ReflectionProp.h"



#ifdef EDITOR_BUILD
class IEditorTool;

extern IEditorTool* level_editor_factory();
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

	IEditorTool* tool_to_edit_me() const override { return g_editor_doc; }

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

	IEditorTool* tool_to_edit_me() const override { return g_editor_doc; }

	virtual const ClassTypeInfo* get_asset_class_type() const { return &PrefabAsset::StaticType; }

	const char* get_arg_for_editortool() const { return "prefab"; }
};
static AutoRegisterAsset<PrefabAssetMetadata> prefab_register_0987;
#endif

SceneAsset::~SceneAsset() {
}
SceneAsset::SceneAsset(){

}
PrefabAsset::PrefabAsset(){

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

bool SceneAsset::load_asset(IAssetLoadingInterface* load)
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
		double start = GetTime();
		sceneFile = std::make_unique<UnserializedSceneFile>(unserialize_entities_from_text(text, load,nullptr));
		printf("level time: %f\n", GetTime() - start);
	}
	catch (int) {
		sys_print(Error, "error loading SceneAsset %s\n", path.c_str());
		return false;
	}

	return true;
}

PrefabAsset::~PrefabAsset() {
}

bool PrefabAsset::load_asset(IAssetLoadingInterface* load)
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
		sceneFile = std::make_unique<UnserializedSceneFile>(unserialize_entities_from_text(text,load, this));
		// add instance ids here for diff'ing entity references
		uint64_t id = 1ull << 63ull;
		for (auto& obj : sceneFile->get_objects()) {
			obj.second->post_unserialization(++id);
			instance_ids_for_diffing.insert(id, obj.second);
		}
		sceneFile->unserialize_post_assign_ids();
	}
	catch (...) {
		sys_print(Error, "error loading PrefabAsset %s\n", path.c_str());
		return false;
	}

	return true;
}
void PrefabAsset::uninstall()
{
	if (!sceneFile)
		return;

	sys_print(Debug, "prefab uninstalled %s\n", get_name().c_str());
	for (auto& o : sceneFile->get_objects())
		delete o.second;
	sceneFile.reset(nullptr);
}

#include "Framework/PropertyUtil.h"

void PrefabAsset::sweep_references(IAssetLoadingInterface* load) const
{
	if (!sceneFile)
		return;

	sys_print(Debug, "prefab sweep ref %s\n", get_name().c_str());
	{
		for (auto& obj : sceneFile->get_objects()) {
			check_object_for_asset_ptr(obj.second, load);
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
