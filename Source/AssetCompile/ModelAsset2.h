#pragma once

#include "Game/SoftAssetPtr.h"
#include "Render/MaterialPublic.h"

#include "SkeletonAsset.h"

#include "Render/Model.h"

#include "MiscEditors/DataClass.h"
#include "Framework/Curve.h"
#include "Animation/Event.h"
#include "Animation/AnimationSeqAsset.h"

#include "Framework/ArrayReflection.h"
#include "Game/SerializePtrHelpers.h"
#include "Game/AssetPtrArrayMacro.h"
#include "Game/AssetPtrMacro.h"
#include "Framework/Reflection2.h"
#include "MiscEditors/DataClass.h"
#include "Game/SoftAssetPtr.h"

template<>
struct GetAtomValueWrapper<std::unique_ptr<AnimationEvent>> {
	static PropertyInfo get() {
		PropertyInfo pi;
		pi.offset = 0;
		pi.name = "_value";
		pi.type = core_type_id::StdUniquePtr;
		pi.flags = PROP_DEFAULT;
		return pi;
	}
};


NEWCLASS(BoneRenameContainer, ClassBase)
public:
	REFLECT();
	std::vector<std::string> remap;
};
NEWCLASS(BoneReparentContainer, ClassBase)
public:
	REFLECT();
	std::vector<std::string> remap;
};


CLASS_H(AnimImportSettings, ClassBase)
public:
	std::string clipName;
	SoftAssetPtr<AnimationSeqAsset> otherClipToSubtract;

	// import settings
	bool hasEndCrop = false;
	bool hasStartCrop = false;
	int cropStart = 0;
	int cropEnd =0;	// a big number
	bool fixLoop = false;
	bool makeAdditive = false;
	bool additiveFromSelf = false;
	bool removeLinearVelocity = false;	// if true, then subtracts linear_velocity*t from each position
	bool enableRootMotion = false;		// if true, then marks clip for root motion, note that you shouldnt use removeLinearVelocity or setRootToFirstPose
	bool setRootToFirstPose = false;	// if true, then sets all root poses to first frame

	std::vector<AnimationEvent*> events;
	std::vector<EditingCurve> curves;

	static const PropertyInfoList* get_props() {
		//MAKE_VECTORCALLBACK_ATOM(std::unique_ptr<AnimationEvent>, events);
		MAKE_VECTORCALLBACK(EditingCurve, curves);
		START_PROPS(AnimImportSettings)
			REG_STDSTRING(clipName, PROP_SERIALIZE),
			REG_BOOL(hasStartCrop, PROP_DEFAULT, "0"),
			REG_INT(cropStart, PROP_DEFAULT, "0"),
			REG_BOOL(hasEndCrop,PROP_DEFAULT,"0"),
			REG_INT(cropEnd, PROP_DEFAULT, "20000"),
			REG_BOOL(fixLoop, PROP_DEFAULT, "0"),
			REG_BOOL(makeAdditive, PROP_DEFAULT, "0"),
			REG_BOOL(additiveFromSelf, PROP_DEFAULT, "0"),
			REG_BOOL(removeLinearVelocity,PROP_DEFAULT,"0"),
			REG_BOOL(enableRootMotion,PROP_DEFAULT,"0"),
			REG_BOOL(setRootToFirstPose,PROP_DEFAULT,"0"),
			REG_SOFT_ASSET_PTR(otherClipToSubtract,PROP_DEFAULT),
			REG_STDVECTOR(curves,PROP_SERIALIZE),
			//REG_STDVECTOR(events,PROP_SERIALIZE),
		END_PROPS(AnimImportSettings)
	}
};

CLASS_H(ModelImportSettings, ClassBase)
public:
	std::string srcGlbFile;									// what .glb file did this come from
	// Mesh data
	std::vector<float> lodScreenSpaceSizes;					// array of lod sizes
	std::vector<AssetPtr<MaterialInstance>> myMaterials;

	// Skeleton data
	bool useSharedSkeleton = false;							// use another skeleton defined in shareSkeletonWithThis
	AssetPtr<Model> shareSkeletonWithThis;		// an optional ModelAsset to share skeletons with
	AssetPtr<DataClass> mirrorTableAsset;			// this is a SkeletonMirror object ptr, fixme needs better type hints
	std::vector<std::string> keepBones;						// array of bones to keep (compilier automatically prunes out unused bones)
	std::vector<std::string> curveNames;					// array of strings that can be used to name custom curves for animations
	std::vector<std::string> additionalAnimationGlbFiles;	// additional glb files to source animations from (will retarget)
	std::vector<AnimImportSettings> animations;				// all animations indexed by string with import settings


	// type=BoneRenameContainer
	// this renames bones in this asset using this dataclass
	// each entry in the array is "my_current_bone renamed_bone", with a space in between
	AssetPtr<DataClass> bone_rename_dataclass;
	AssetPtr<DataClass> bone_reparent;
	float animations_set_fps = 30.0;

	static const PropertyInfoList* get_props() {

		MAKE_VECTORCALLBACK_ATOM(float, lodScreenSpaceSizes);
		MAKE_VECTORCALLBACK_ATOM(std::string, additionalAnimationGlbFiles);
		MAKE_VECTORCALLBACK_ATOM(AssetPtr<MaterialInstance>, myMaterials);
		MAKE_VECTORCALLBACK_ATOM(std::string, keepBones);
		MAKE_VECTORCALLBACK(AnimImportSettings, animations);
		START_PROPS(ModelImportSettings)
			REG_STDSTRING(srcGlbFile, PROP_DEFAULT),
			REG_STDVECTOR(myMaterials, PROP_DEFAULT),
			REG_STDVECTOR(lodScreenSpaceSizes, PROP_DEFAULT),
			REG_BOOL(useSharedSkeleton, PROP_DEFAULT, "0"),
			REG_ASSET_PTR(shareSkeletonWithThis, PROP_DEFAULT),
			REG_STDVECTOR(keepBones, PROP_DEFAULT),
			REG_ASSET_PTR(mirrorTableAsset, PROP_DEFAULT),
			REG_STDVECTOR(additionalAnimationGlbFiles, PROP_DEFAULT),
			REG_ASSET_PTR(bone_rename_dataclass,PROP_DEFAULT),
			REG_ASSET_PTR(bone_reparent, PROP_DEFAULT),
			REG_FLOAT(animations_set_fps, PROP_DEFAULT, "30.0"),
			REG_STDVECTOR(animations, PROP_SERIALIZE),
		END_PROPS(ModelImportSettings)
	}
};