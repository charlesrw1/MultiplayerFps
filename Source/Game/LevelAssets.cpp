#include "LevelAssets.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"
#include "Game/BaseUpdater.h"
#include "Framework/ReflectionProp.h"
#include "LevelEditor/EditorDocLocal.h"
#include "Framework/MapUtil.h"
#include "LevelSerialization/SerializeNew.h"
#include "Framework/StringUtils.h"
#include <string>
using std::make_unique;
#ifdef EDITOR_BUILD
class IEditorTool;
using std::string;
extern ConfigVar g_editor_newmap_template;

extern void post_load_map_callback_generic(bool make_plane);



extern IEditorTool* level_editor_factory();
//extern IEditorTool* g_editor_doc;
class MapAssetMetadata : public AssetMetadata
{
public:
	MapAssetMetadata() {
		extensions.push_back("tmap");

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

	//IEditorTool* tool_to_edit_me() const override { return g_editor_doc; }

	virtual const ClassTypeInfo* get_asset_class_type() const { return nullptr; }



};

static AutoRegisterAsset<MapAssetMetadata> map_register_0987;




void SchemaManager::init() {
	auto ents = FileSys::open_read_game("ent_schema.json");
	if (ents) {
		string textForm = std::string(ents->size(), ' ');
		ents->read((void*)textForm.data(), textForm.size());
		schema_file = nlohmann::json::parse(textForm);
	}
	else {
		sys_print(Warning, "no entity schema (ent_schema.json)\n");
	}
}


class SpawnerAssetMeta : public AssetMetadata
{
public:
	SpawnerAssetMeta() {
	}

	// Inherited via AssetMetadata
	virtual Color32 get_browser_color()  const override
	{
		return { 255, 117, 133 };
	}

	virtual std::string get_type_name() const  override
	{
		return "Spawner-Entity";
	}

	//IEditorTool* tool_to_edit_me() const override { return g_editor_doc; }

	virtual const ClassTypeInfo* get_asset_class_type() const { return nullptr; }
	void fill_extra_assets(std::vector<std::string>& out) const final {
		auto& json = SchemaManager::get().schema_file;
		for (auto&[name,dict] : json.items()) {
			out.push_back(name);
		}
	}
};
static AutoRegisterAsset<SpawnerAssetMeta> schefab_register_0987;


#endif

//YOU ARE GARBAGE
ConfigVar g_prefab_factory("g_prefab_factory", "", CVAR_DEV, "");



string get_string_from_file(IFile* fileptr)
{
	string textForm = std::string(fileptr->size(), ' ');
	fileptr->read((void*)textForm.data(), textForm.size());
	if (StringUtils::starts_with(textForm, "!json\n")) {
		textForm = textForm.substr(5);
	}
	return textForm;
}

uptr<UnserializedSceneFile> load_level_asset(string path)
{
	auto fileptr = FileSys::open_read_game(path.c_str());
	if (!fileptr) {
		sys_print(Error, "SceneAsset::load_asset: couldn't open scene %s\n", path.c_str());
		return nullptr;
	}
	string textForm = get_string_from_file(fileptr.get());
	SerializedForDiffing blah;
	try {
		blah.jsonObj = nlohmann::json::parse(textForm);
	}
	catch (...) {
		sys_print(Error, "error parsing json %s\n", path.c_str());
		return nullptr;
	}
	UnserializedSceneFile* out = new UnserializedSceneFile(
		NewSerialization::unserialize_from_json(path.c_str()/*debug tag*/, blah, *g_assets.loader, false)
	);

	return uptr<UnserializedSceneFile>(out);
}

