#pragma once
#include "Framework/ClassBase.h"
#include <string>
#include <vector>
#include "Framework/ReflectionMacros.h"
#include "Framework/ArrayReflection.h"
#include "Assets/IAsset.h"
#include "Assets/AssetLoaderRegistry.h"
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
// an "alias" asset meant to refernce an animation clip to use with the asset browser
CLASS_H(AnimationSeqAsset, IAsset)
public:
	// get_name() is the animation name
	Model* srcModel = nullptr;
	const AnimationSeq* seq = nullptr;
	friend class AnimationSeqLoader;
};

class AnimationSeqLoader : public IAssetLoader
{
public:
	void init();
	IAsset* load_asset(const std::string& file) {
		return find_animation_seq_and_load_model(file);
	}
	AnimationSeqAsset* find_animation_seq_and_load_model(const std::string& seq);
	void update_manifest_with_model(const std::string& modelName, const std::vector<std::string>& animNames);
private:
	AnimationListManifest* manifest = nullptr;
	std::unordered_map<std::string, AnimationSeqAsset*> animNameToSeq;

	friend class AnimationSeqAssetMetadata;
};

extern AnimationSeqLoader g_animseq;