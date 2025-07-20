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



	uptr<CreateEditorAsync> create_create_tool_to_edit(opt<string> assetPath) const { 
		return make_unique<CreateLevelEditorAync>(assetPath); 
	}

};

static AutoRegisterAsset<MapAssetMetadata> map_register_0987;

//YOU ARE GARBAGE
ConfigVar g_prefab_factory("g_prefab_factory", "", CVAR_DEV, "");

void PrefabAsset::init_prefab_factory(){
	if (PrefabAsset::factory) {
		delete factory;
		factory = nullptr;
	}
	factory = ClassBase::create_class<IPrefabFactory>(g_prefab_factory.get_string());
	if (factory) {
		factory->start();
	}
}
IPrefabFactory* PrefabAsset::factory = nullptr;

class PrefabAssetMetadata : public AssetMetadata
{
public:
	PrefabAssetMetadata() {
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

	//IEditorTool* tool_to_edit_me() const override { return g_editor_doc; }

	virtual const ClassTypeInfo* get_asset_class_type() const { return &PrefabAsset::StaticType; }
	void fill_extra_assets(std::vector<std::string>& out) const final {
		if (PrefabAsset::factory) {
			for (auto& name : PrefabAsset::factory->defined_prefabs) {
				out.push_back(name);
			}
		}
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

}


bool PrefabAsset::load_asset(IAssetLoadingInterface* load)
{
	auto& path = get_name();
	if (!factory) return false;
	if (!SetUtil::contains(factory->defined_prefabs, path)) {
		return false;
	}
	return true;
}
void PrefabAsset::uninstall()
{

}
void PrefabAsset::finish_prefab_setup(Entity* me) const {
	if (PrefabAsset::factory) {
#ifdef EDITOR_BUILD
		std::unordered_set<void*> ignore;
		for (auto c : me->get_children())
			ignore.insert((void*)c);
		for (auto c : me->get_components())
			ignore.insert((void*)c);
#endif

		string name = get_name();
		StringUtils::remove_extension(name);
		PrefabAsset::factory->create(me, name);

#ifdef EDITOR_BUILD
		auto set_recursive = [](auto&& self, Entity* e,const std::unordered_set<void*>& ignore) -> void {
			for (auto c : e->get_components()) {
				if (!SetUtil::contains(ignore, (void*)c)) {
					c->dont_serialize_or_edit = true;
				}
			}
			for (auto c : e->get_children()) {
				if (!SetUtil::contains(ignore, (void*)c)) {
					c->dont_serialize_or_edit = true;
					self(self, c, ignore);
				}
			}
		};
		set_recursive(set_recursive, me, ignore);
#endif

	}
}

#include "Framework/PropertyUtil.h"


void PrefabAsset::move_construct(IAsset* other)
{
	sys_print(Debug, "PrefabAsset::move_construct: %s\n", get_name().c_str());
	PrefabAsset* o = (PrefabAsset*)other;

}
PrefabAsset* PrefabAsset::load(string s) {
	return g_assets.find_sync<PrefabAsset>(s).get();
}
void PrefabAsset::post_load() {

}