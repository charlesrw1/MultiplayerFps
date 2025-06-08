#pragma once
#include "Assets/IAsset.h"
#include "Game/SerializePtrHelpers.h"
#include "Render/MaterialPublic.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ArrayReflection.h"
#include "Framework/StructReflection.h"
// Decided to make these plain DataClass objects instead of IAssets for simplicity
using std::string;
struct BoneMirror {
	STRUCT_BODY();
	REF string boneA;
	REF string boneB;
};

class SkeletonMirror : public ClassBase {
public:
	CLASS_BODY(SkeletonMirror);
	REF std::vector<BoneMirror> mirrors;
};
struct BoneMaskValue {
	STRUCT_BODY();
	REF string bone;
	REF float weight = 1.0;
};
class SkeletonMask : public ClassBase {
public:
	CLASS_BODY(SkeletonMask);
	REF std::vector<BoneMaskValue> masks;
};