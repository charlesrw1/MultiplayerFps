#pragma once

#include "Render/MaterialPublic.h"

#include "SkeletonAsset.h"

#include "Render/Model.h"

#include "Framework/Curve.h"
#include "Animation/Event.h"
#include "Animation/AnimationSeqAsset.h"

#include "Framework/ArrayReflection.h"

#include "Framework/Reflection2.h"

struct BoneRenameContainer
{
	STRUCT_BODY();
	REF std::vector<std::string> remap;
};

struct BoneReparentContainer
{
	STRUCT_BODY();
	REF std::vector<std::string> remap;
};

struct BoneRetargetName
{
	STRUCT_BODY();
	REF std::string bone_name;
	REF int type = 0;
};

struct AnimImportSettings
{
	STRUCT_BODY();

	REF std::string clipName;
	REF std::string otherClipToSubtract;

	// import settings
	REF bool hasEndCrop = false;
	REF bool hasStartCrop = false;
	REF int cropStart = 0;
	REF int cropEnd = 0; // a big number
	REF bool fixLoop = false;
	REF bool makeAdditive = false;
	REF bool additiveFromSelf = false;
	REF int additiveSelfFrame = 0; // when additiveFromSelf, subtract this frame of the clip (default = first frame)
	REF bool removeLinearVelocity = false; // if true, then subtracts linear_velocity*t from each position
	REF bool enableRootMotion = false;	   // if true, then marks clip for root motion, note that you shouldnt use
										   // removeLinearVelocity or setRootToFirstPose
	REF bool setRootToFirstPose = false;   // if true, then sets all root poses to first frame
};

class ModelImportSettings : public ClassBase
{
public:
	CLASS_BODY(ModelImportSettings);

	REF std::string srcGlbFile; // what .glb file did this come from
	// Mesh data
	REF std::vector<float> lodScreenSpaceSizes; // array of lod sizes
	REF std::vector<AssetPtr<MaterialInstance>> myMaterials;

	// Skeleton data
	REF bool useSharedSkeleton = false;		   // use another skeleton defined in shareSkeletonWithThis
	REF AssetPtr<Model> shareSkeletonWithThis; // an optional ModelAsset to share skeletons with
	// REF AssetPtr<DataClass> mirrorTableAsset;			// this is a SkeletonMirror object ptr, fixme needs better
	// type hints
	REF std::vector<std::string> keepBones;	 // array of bones to keep (compilier automatically prunes out unused bones)
	REF bool disablePruneUnusedBones = false; // if true, keep all bones (don't prune unused/unreferenced bones)
	REF std::vector<std::string> curveNames; // array of strings that can be used to name custom curves for animations

	// additional glb files to source animations from (will retarget)
	REF std::vector<std::string> additionalAnimationGlbFiles;
	REF std::vector<AnimImportSettings> animations; // all animations indexed by string with import settings
	REF std::vector<BoneRetargetName> bone_retargets;

	REF int lightmapSizeX = 0;
	REF int lightmapSizeY = 0;
	REF bool withLightmap = false;
	REF bool worldLmMerge = false;
	REF bool meshAsConvex = false;
	REF bool meshAsCollision = false;

	REF bool generate_auto_lods = false;

	REF float animations_set_fps = 30.0;

	REF BoneRenameContainer bone_rename;
	REF BoneReparentContainer bone_reparent;

};