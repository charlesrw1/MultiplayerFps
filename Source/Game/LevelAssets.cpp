#include "LevelAssets.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"
#include "Game/BaseUpdater.h"
#include "LevelSerialization/SerializationAPI.h"
#include "Framework/ReflectionProp.h"
#include "EngineSystemCommands.h"
#include "LevelEditor/EditorDocLocal.h"
#include "LevelSerialization/SerializeNewMakers.h"
#include "Framework/MapUtil.h"
#include "LevelSerialization/SerializeNew.h"
#include "Framework/StringUtils.h"

using std::make_unique;
#ifdef EDITOR_BUILD
class IEditorTool;

extern ConfigVar g_editor_newmap_template;

extern void post_load_map_callback_generic(bool make_plane);

class CreateLevelEditorAync : public CreateEditorAsync {
public:
	CreateLevelEditorAync(opt<string> assetPath) : assetPath(assetPath) {}
	void execute(Callback callback) final {
		assert(callback);
		uptr<OpenMapCommand> cmd;
		const bool wants_new_map = !assetPath.has_value();
		if (wants_new_map) {
			cmd = make_unique<OpenMapCommand>(std::nullopt, false/* for editor */);
		}
		else {
			cmd = make_unique<OpenMapCommand>(assetPath, false/* for editor */);
		}
		const opt<string> assetPath = this->assetPath;
		cmd->callback = [callback, assetPath](OpenMapReturnCode code) {
			if (code == OpenMapReturnCode::Success) {
				assert(eng->get_level());
				uptr<EditorDoc> editorDoc(EditorDoc::create_scene(assetPath));
				callback(std::move(editorDoc));
			}
			else {
				sys_print(Warning, "CreateLevelEditorAync::execute: failed to load map (%s)\n",assetPath.value_or("<unnamed>").c_str());
				callback(nullptr);
			}
		};
		Cmd_Manager::inst->append_cmd(std::move(cmd));
	}
	string get_tab_name() final {
		return assetPath.value_or("UnnamedMap");
	}
	opt<string> get_asset_name() final {
		return assetPath;
	}

	opt<string> assetPath;
};

class CreatPrefabEditorAync : public CreateEditorAsync {
public:
	CreatPrefabEditorAync(opt<string> assetPath) : assetPath(assetPath) {}
	void execute(Callback callback) final {
		assert(callback);
		opt<string> assetPath = this->assetPath;
		string newmap_template = g_editor_newmap_template.get_string();
		uptr<OpenMapCommand> cmd = make_unique<OpenMapCommand>(std::nullopt, false/* for editor */);
		cmd->callback = [callback, assetPath](OpenMapReturnCode code) {
			uptr<EditorDoc> editorDoc;
			if (code==OpenMapReturnCode::Success) {
				assert(eng->get_level());
				post_load_map_callback_generic(false);
				if (assetPath.has_value()) {
					PrefabAsset* prefab = g_assets.find_sync<PrefabAsset>(assetPath.value()).get();
					if(prefab)
						editorDoc.reset(EditorDoc::create_prefab(prefab));
				}
				else {
					editorDoc.reset(EditorDoc::create_prefab(nullptr));
				}
			}
			else {
				sys_print(Warning, "CreatPrefabEditorAync::execute: failed to load map (%s)\n", assetPath.value_or("<unnamed>").c_str());
			}

			callback(std::move(editorDoc));
		};
		Cmd_Manager::inst->append_cmd(std::move(cmd));
	}
	string get_tab_name() final {
		return assetPath.value_or("UnnamedPrefab");
	}
	opt<string> get_asset_name() final {
		return assetPath;
	}
	opt<string> assetPath;
};



extern IEditorTool* level_editor_factory();
//extern IEditorTool* g_editor_doc;
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

	//IEditorTool* tool_to_edit_me() const override { return g_editor_doc; }

	virtual const ClassTypeInfo* get_asset_class_type() const { return &SceneAsset::StaticType; }

	const char* get_arg_for_editortool() const { return "scene"; }

	uptr<CreateEditorAsync> create_create_tool_to_edit(opt<string> assetPath) const { 
		return make_unique<CreateLevelEditorAync>(assetPath); 
	}

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

	//IEditorTool* tool_to_edit_me() const override { return g_editor_doc; }

	virtual const ClassTypeInfo* get_asset_class_type() const { return &PrefabAsset::StaticType; }

	const char* get_arg_for_editortool() const { return "prefab"; }

	uptr<CreateEditorAsync> create_create_tool_to_edit(opt<string> assetPath) const {
		return make_unique<CreatPrefabEditorAync>(assetPath);
	}
};
static AutoRegisterAsset<PrefabAssetMetadata> prefab_register_0987;
#endif

SceneAsset::~SceneAsset() {
	sys_print(Debug, "~SceneAsset: %s\n", get_name().c_str());
}
SceneAsset::SceneAsset(){

}
PrefabAsset::PrefabAsset(){

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
	
	sceneFile = std::make_unique<UnserializedSceneFile>(NewSerialization::unserialize_from_json(get_name().c_str(), *halfUnserialized, *g_assets.loader));
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

PrefabAsset::~PrefabAsset() {
	sys_print(Debug, "~PrefabAsset: %s\n", get_name().c_str());
	if (sceneFile)
		sceneFile->delete_objs();
}


UnserializedSceneFile PrefabAsset::unserialize(IAssetLoadingInterface* load) const
{
	assert(halfUnserialized);
	if (!load) 
		load = g_assets.loader;
	double start = GetTime();
	UnserializedSceneFile out = NewSerialization::unserialize_from_json(get_name().c_str(), *halfUnserialized, *load);
	double now = GetTime();
	sys_print(Debug, "PrefabAsset::unserialize: took %f\n", float(now - start));
	return out;
}
bool PrefabAsset::load_asset(IAssetLoadingInterface* load)
{
	auto& path = get_name();
	auto fileptr = FileSys::open_read_game(path.c_str());
	if (!fileptr) {
		sys_print(Error, "PrefabAsset::load_asset: couldn't open scene %s\n", path.c_str());
		return false;
	}
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
		sys_print(Error, "PrefabAsset::load_asset: error %s\n", path.c_str());
		return false;
	}
	assert(halfUnserialized);

	return true;
}
void PrefabAsset::uninstall()
{
	if (!sceneFile)
		return;
	sceneFile->delete_objs();
	sceneFile.reset(nullptr);
}

#include "Framework/PropertyUtil.h"


void PrefabAsset::move_construct(IAsset* other)
{
	sys_print(Debug, "PrefabAsset::move_construct: %s\n", get_name().c_str());
	if (sceneFile.get()) {
		PrefabAsset::uninstall();
	}
	ASSERT(!sceneFile.get());
	PrefabAsset* o = (PrefabAsset*)other;
	sceneFile = std::move(o->sceneFile);
	halfUnserialized = std::move(o->halfUnserialized);
	//text = std::move(o->text);
	instance_ids_for_diffing = std::move(o->instance_ids_for_diffing);
}
PrefabAsset* PrefabAsset::load(string s) {
	return g_assets.find_sync<PrefabAsset>(s).get();
}
void PrefabAsset::post_load() {
	sceneFile = std::make_unique<UnserializedSceneFile>(unserialize(g_assets.loader));
	if (sceneFile->num_roots != 1) {
		sys_print(Error, "PrefabAsset::load_asset: prefab doesnt have 1 root, has: %d\n", sceneFile->num_roots);
		throw std::runtime_error("load error");
	}

	// add instance ids here for diff'ing entity references
	uint64_t id = 1ull << 62ull;
	for (auto& obj : sceneFile->all_obj_vec) {
		obj->post_unserialization(++id);
		instance_ids_for_diffing.insert(id, obj);
	}
	sceneFile->unserialize_post_assign_ids();
}