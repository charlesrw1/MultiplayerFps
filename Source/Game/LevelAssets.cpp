#include "LevelAssets.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetRegistry.h"

#include "Framework/Files.h"

CLASS_IMPL(SceneAsset);
CLASS_IMPL(PrefabAsset);


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
	catch (...) {
		sys_print(Error, "error loading SceneAsset %s\n", path.c_str());
		return false;
	}

	return true;
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
		sceneFile = std::make_unique<UnserializedSceneFile>(unserialize_entities_from_text(text));
	}
	catch (...) {
		sys_print(Error, "error loading PrefabAsset %s\n", path.c_str());
		return false;
	}

	return true;
}

