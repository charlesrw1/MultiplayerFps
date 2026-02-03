#include "LevelAssets.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"
#include "Game/BaseUpdater.h"
#include "Framework/ReflectionProp.h"
#include "EngineSystemCommands.h"
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

	virtual const ClassTypeInfo* get_asset_class_type() const { return &SceneAsset::StaticType; }



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


SceneAsset::~SceneAsset() {
	sys_print(Debug, "~SceneAsset: %s\n", get_name().c_str());
}
SceneAsset::SceneAsset(){

}


void SceneAsset::uninstall() {

	if (sceneFile.get()) {
		sys_print(Warning, "scene asset with non-null scenefile\n");
		for (auto& o : sceneFile->all_obj_vec)
			delete o;
		sceneFile.reset(nullptr);
	}
}
void SceneAsset::move_construct(IAsset*) {
	sys_print(Warning, "scene asset move construct, shouldnt have happened\n");
}



void SceneAsset::post_load()
{
	// hmm move this to post_load on main thread because lua isnt thread safe
	// use asset bundles to do the models,textures, etc. on an async thread
	// this also throws on error, catch it in assets
	double start = GetTime();
	

	sceneFile = std::make_unique<UnserializedSceneFile>(NewSerialization::unserialize_from_json(get_name().c_str(), *halfUnserialized, *g_assets.loader, false));

	double now = GetTime();
	sys_print(Debug, "SceneAsset::post_load: took %f\n", float(now - start));
}
bool SceneAsset::load_asset(IAssetLoadingInterface* load)
{
	auto& path = get_name();

	auto fileptr = FileSys::open_read_game(path.c_str());
	if (!fileptr) {
		sys_print(Error, "SceneAsset::load_asset: couldn't open scene %s\n", path.c_str());
		return false;
	}
	double start = GetTime();

	string textForm = std::string(fileptr->size(), ' ');
	fileptr->read((void*)textForm.data(), textForm.size());
	if (StringUtils::starts_with(textForm, "!json\n")) {
		textForm = textForm.substr(5);
	}
	try {
		halfUnserialized = make_unique<SerializedForDiffing>();
		halfUnserialized->jsonObj = nlohmann::json::parse(textForm);
	}
	catch (...) {
		sys_print(Error, "SceneAsset::load_asset: json unserialize error %s\n", path.c_str());
		return false;
	}

	double now = GetTime();
	sys_print(Debug, "SceneAsset::load_asset: took %f\n", float(now - start));
	return true;
}
