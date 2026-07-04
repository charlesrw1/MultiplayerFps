#pragma once
#include "Framework/ClassBase.h"
#include <string>
#include <vector>
#include "Framework/ReflectionMacros.h"
#include "Framework/ArrayReflection.h"
#include "Assets/IAsset.h"

#include "Render/Model.h"
class Model;
class AnimationSeq;
// an "alias" asset meant to refernce an animation clip inside a model to use with the asset browser and asset ptr's
class AnimationSeqAsset : public IAsset
{
public:
	CLASS_BODY(AnimationSeqAsset);

	REF static AnimationSeqAsset* load(const std::string& name);


	void uninstall() override;
	void post_load() override {}
	bool load_asset() override;
	std::shared_ptr<Model> srcModel;
	const AnimationSeq* seq = nullptr;
	// clip name portion of this asset's path (everything after the last '/').
	std::string get_clip_name() const;
};