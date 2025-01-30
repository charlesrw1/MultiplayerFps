#pragma once
#include "Assets/IAsset.h"
#include "Game/SerializePtrHelpers.h"
#include "Render/MaterialPublic.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ArrayReflection.h"

// Decided to make these plain DataClass objects instead of IAssets for simplicity
CLASS_H(SkeletonMirror, ClassBase)
public:
	struct BoneMirror {
		std::string boneA;
		std::string boneB;
		static PropertyInfoList* get_props() {
			START_PROPS(BoneMirror)
				REG_STDSTRING(boneA, PROP_DEFAULT),
				REG_STDSTRING(boneB, PROP_DEFAULT),
			END_PROPS(BoneMirror)
		}
	};
	std::vector<BoneMirror> mirrors;

	static const PropertyInfoList* get_props() {
		MAKE_VECTORCALLBACK(BoneMirror, mirrors);
		START_PROPS(SkeletonMirror)
			REG_STDVECTOR(mirrors, PROP_DEFAULT)
		END_PROPS(SkeletonMirror)
	}
};
CLASS_H(SkeletonMask, ClassBase)
public:
	struct BoneFloat {
		std::string bone;
		float weight = 1.0;

		static PropertyInfoList* get_props() {
			START_PROPS(BoneFloat)
				REG_STDSTRING(bone, PROP_DEFAULT),
				REG_FLOAT(weight, PROP_DEFAULT, "1")
			END_PROPS(BoneFloat)
		}
	};
	std::vector<BoneFloat> masks;

	static const PropertyInfoList* get_props() {
		MAKE_VECTORCALLBACK(BoneFloat, masks);
		START_PROPS(SkeletonMask)
			REG_STDVECTOR(masks, PROP_DEFAULT)
		END_PROPS(SkeletonMask)
	}
};