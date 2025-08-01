#include "AnimationSeqAsset.h"
#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"
#include "Render/Model.h"
#include "Animation/SkeletonData.h"
#include "Assets/AssetDatabase.h"
#include <fstream>

#ifdef EDITOR_BUILD
//extern IEditorTool* g_animseq_editor;
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

	virtual void fill_extra_assets(std::vector<std::string>& filepaths) const  override
	{	
		for (auto& f : FileSys::find_game_files()) {
			if (get_extension(f) == ".anims") {
				std::ifstream infile(f);
				std::string a;
				std::string base = strip_extension(FileSys::get_game_path_from_full_path(f)) + "/";
				while (std::getline(infile, a)) {
					filepaths.push_back(base + a);
				}
			}
		}
	}
	//virtual IEditorTool* tool_to_edit_me() const override {
	//	return g_animseq_editor; 
	//}


	virtual const ClassTypeInfo* get_asset_class_type() const { return &AnimationSeqAsset::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(AnimationSeqAssetMetadata);
#endif

bool AnimationSeqAsset::load_asset(IAssetLoadingInterface* load)
{
	auto& path = get_name();
	auto pos = path.rfind('/');
	if (pos == std::string::npos) {
		sys_print(Error, "no forward slash in animation seq\n");
		return false;
	}
	std::string modName = path.substr(0, pos) + ".cmdl";
	std::string animName = path.substr(pos + 1);

	//auto m = load->load_asset(&Model::StaticType, modName);
	srcModel = g_assets.find_sync_sptr<Model>(modName);// m->cast_to<Model>();
	if (srcModel && srcModel->get_skel()) {
		seq = srcModel->get_skel()->find_clip(animName);



		return true;
	}
	else
		return false;
}


void AnimationSeqAsset::move_construct(IAsset* _other) {
	auto other = (AnimationSeqAsset*)_other;

	*this = std::move(*other);
}
void AnimationSeqAsset::uninstall()
{

}