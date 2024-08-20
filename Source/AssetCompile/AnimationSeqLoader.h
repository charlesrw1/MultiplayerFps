#pragma once
#include "Framework/ClassBase.h"
#include <string>
#include <vector>
#include "Framework/ReflectionMacros.h"
#include "Framework/ArrayReflection.h"
#include "Assets/IAsset.h"
#include "Game/SerializePtrHelpers.h"
CLASS_H(AnimationListManifest, ClassBase)
public:
	struct Item {
		std::string name;
		std::vector<std::string> animList;

		static PropertyInfoList* get_props() {
			MAKE_VECTORCALLBACK_ATOM(std::string, animList);
			START_PROPS(Item)
				REG_STDSTRING(name,PROP_DEFAULT),
				REG_STDVECTOR(animList,PROP_DEFAULT)
			END_PROPS(Item)
		}
	};
	std::vector<Item> items;

	static const PropertyInfoList* get_props() {
		MAKE_VECTORCALLBACK(Item, items);
		START_PROPS(AnimationListManifest)
			REG_STDVECTOR(items, PROP_DEFAULT)
		END_PROPS(AnimationListManifest)
	}
};
class Model;
class AnimationSeq;
// an "alias" asset meant to refernce an animation clip inside a model to use with the asset browser and asset ptr's
CLASS_H(AnimationSeqAsset, IAsset)
public:
	void uninstall() override {
		srcModel = nullptr;
		seq = nullptr;
	}
	void post_load(ClassBase* user) override {}
	bool load_asset(ClassBase*& user) override;
	void sweep_references() const override;
	void move_construct(IAsset* _other) override {
		auto other = (AnimationSeqAsset*)_other;
		*this = std::move(*other);
	}

	// get_name() is the animation name
	AssetPtr<Model> srcModel;
	const AnimationSeq* seq = nullptr;
	friend class AnimationSeqLoader;
};

class AnimationSeqLoader
{
public:
	void init();
	void update_manifest_with_model(const std::string& modelName, const std::vector<std::string>& animNames);
private:
	AnimationListManifest* manifest = nullptr;

	friend class AnimationSeqAssetMetadata;
};

extern AnimationSeqLoader g_animseq;