#pragma once
#include "Framework/ClassBase.h"
#include <string>
#include <vector>
#include "Framework/ReflectionMacros.h"
#include "Framework/ArrayReflection.h"
#include "Assets/IAsset.h"
#include "Game/SerializePtrHelpers.h"
#include "Render/Model.h"
class Model;
class AnimationSeq;
// an "alias" asset meant to refernce an animation clip inside a model to use with the asset browser and asset ptr's
class AnimationSeqAsset : public IAsset {
public:
	CLASS_BODY(AnimationSeqAsset);
	void uninstall() override;
	void post_load() override {}
	bool load_asset(IAssetLoadingInterface* load) override;
	void move_construct(IAsset* _other) override;
	std::shared_ptr<Model> srcModel;
	const AnimationSeq* seq = nullptr;
};