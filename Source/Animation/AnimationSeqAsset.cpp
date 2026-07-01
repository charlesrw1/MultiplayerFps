#include "AnimationSeqAsset.h"
#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"
#include "Render/Model.h"
#include "Animation/SkeletonData.h"
#include "Assets/AssetDatabase.h"
#include <fstream>

#ifdef EDITOR_BUILD
// extern IEditorTool* g_animseq_editor;
class AnimationSeqAssetMetadata : public AssetMetadata
{
public:
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const override { return {40, 135, 205}; }

	virtual std::string get_type_name() const override { return "AnimationSeq"; }

	virtual void fill_extra_assets(std::vector<std::string>& filepaths) const override {
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
	// virtual IEditorTool* tool_to_edit_me() const override {
	//	return g_animseq_editor;
	//}

	virtual const ClassTypeInfo* get_asset_class_type() const { return &AnimationSeqAsset::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(AnimationSeqAssetMetadata);
#endif

std::string AnimationSeqAsset::get_clip_name() const {
	auto& path = get_name();
	auto pos = path.rfind('/');
	return pos == std::string::npos ? "" : path.substr(pos + 1);
}

bool AnimationSeqAsset::load_asset() {
	auto& path = get_name();
	auto pos = path.rfind('/');
	if (pos == std::string::npos) {
		sys_print(Error, "no forward slash in animation seq\n");
		return false;
	}
	std::string modName = path.substr(0, pos) + ".cmdl";
	std::string animName = path.substr(pos + 1);

	// auto m = load->load_asset(&Model::StaticType, modName);
	srcModel = g_assets.find_sync_sptr<Model>(modName); // m->cast_to<Model>();
	if (srcModel && srcModel->get_skel()) {
		seq = srcModel->get_skel()->find_clip(animName);

		return true;
	} else
		return false;
}

void AnimationSeqAsset::uninstall() {
	// Drop the cached pointer; load_asset will re-resolve it on reload.
	srcModel.reset();
	seq = nullptr;
}

// AssetDatabase::reload() only reloads the single asset passed to it -- it has no
// dependency graph, so reloading a Model does NOT cascade to AnimationSeqAsset
// instances that resolved `seq` into that model's (now wiped) MSkeleton::clips.
// Subscribe once, globally, so every loaded AnimationSeqAsset stays in sync with
// its source model instead of leaving `seq` dangling until someone happens to
// touch that specific asset. Mirrors the MaterialInstance master->instance cascade
// in MaterialLocal.cpp, just triggered from the referenced type's load event
// instead of the referencing type's own post_load (Model can't depend on Animation).
static const bool s_animseq_reload_on_model_reload = [] {
	static int s_key = 0;
	Model::on_model_loaded.add(&s_key, [](Model* reloaded) {
		if (!reloaded)
			return;
		std::vector<IAsset*> assets;
		g_assets.get_assets_of_type(assets, &AnimationSeqAsset::StaticType);
		for (IAsset* a : assets) {
			auto* seqAsset = static_cast<AnimationSeqAsset*>(a);
			if (seqAsset->srcModel.get() == reloaded)
				g_assets.reload(seqAsset);
		}
	});
	return true;
}();