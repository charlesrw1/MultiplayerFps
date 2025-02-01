#pragma once
#include "Framework/ClassBase.h"
#include <string>
#include <vector>
#include "Framework/ReflectionMacros.h"
#include "Framework/ArrayReflection.h"
#include "Assets/IAsset.h"
#include "Game/SerializePtrHelpers.h"

class Model;
class AnimationSeq;
// an "alias" asset meant to refernce an animation clip inside a model to use with the asset browser and asset ptr's
CLASS_H(AnimationSeqAsset, IAsset)
public:
	void uninstall() override;
	void post_load(ClassBase* user) override {}
	bool load_asset(ClassBase*& user) override;
	void sweep_references() const override;
	void move_construct(IAsset* _other) override;

	// get_name() is the animation name
	AssetPtr<Model> srcModel;
	const AnimationSeq* seq = nullptr;
	friend class AnimationSeqLoader;
};