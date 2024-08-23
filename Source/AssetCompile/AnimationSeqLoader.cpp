#include "AnimationSeqLoader.h"

#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"
#include "Someutils.h"
#include "Framework/ObjectSerialization.h"
AnimationSeqLoader g_animseq;

CLASS_IMPL(AnimationListManifest);
extern IEditorTool* g_animseq_editor;
class AnimationSeqAssetMetadata : public AssetMetadata
{
public:
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const  override
	{
		return { 40, 135, 205 };
	}

	virtual std::string get_type_name() const  override
	{
		return "AnimationSeq";
	}

	virtual void index_assets(std::vector<std::string>& filepaths) const  override
	{
		auto manifest = g_animseq.manifest;


		for (int i = 0; i < manifest->items.size(); i++) {
			auto& item = manifest->items[i];
			for (auto& a : item.animList) {
				filepaths.push_back(strip_extension(item.name) + "/" + a);
			}
		}
	}
	virtual IEditorTool* tool_to_edit_me() const override {
		return g_animseq_editor; 
	}
	virtual bool assets_are_filepaths() const override { return false; }
	virtual std::string root_filepath() const  override
	{
		return "";
	}
	virtual const ClassTypeInfo* get_asset_class_type() const { return &AnimationSeqAsset::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(AnimationSeqAssetMetadata);

CLASS_IMPL(AnimationSeqAsset);

void AnimationSeqLoader::init()
{
	auto file = FileSys::open_read_os("./Data/AnimManifest.txt");
	if (file) {
		DictParser dp;
		dp.load_from_file(file.get());
		StringView tok;
		dp.read_string(tok);
		manifest = read_object_properties<AnimationListManifest>(nullptr, dp, tok);
	}
	if (!manifest) {
		manifest = new AnimationListManifest;
	}
}
#include "Render/Model.h"
#include "Animation/SkeletonData.h"
#include "Assets/AssetDatabase.h"
bool AnimationSeqAsset::load_asset(ClassBase*& user)
{
	auto& path = get_name();
	auto pos = path.rfind('/');
	if (pos == std::string::npos) {
		sys_print("!!! no forward slash in animation seq\n");
		return false;
	}
	std::string modName = path.substr(0, pos) + ".cmdl";
	std::string animName = path.substr(pos + 1);

	srcModel = GetAssets().find_sync<Model>(modName);
	if (srcModel && srcModel->get_skel()) {
		int dummy{};
		seq = srcModel->get_skel()->find_clip(animName, dummy);
		return true;
	}
	else
		return false;
}

void AnimationSeqAsset::sweep_references() const
{
	GetAssets().touch_asset(srcModel.get_unsafe());
}

#include <fstream>
void AnimationSeqLoader::update_manifest_with_model(const std::string& modelName, const std::vector<std::string>& animNames)
{
	bool found = false;
	for (int i = 0; i < manifest->items.size(); i++) {
		if (manifest->items[i].name == modelName) {
			manifest->items.erase(manifest->items.begin() + i);
			break;
		}
	}
	AnimationListManifest::Item i;
	i.name = modelName;
	i.animList = animNames;
	manifest->items.push_back(i);

	DictWriter dw;
	write_object_properties(manifest, nullptr, dw);
	std::ofstream outfile("./Data/AnimManifest.txt");
	outfile.write(dw.get_output().data(), dw.get_output().size());
}
