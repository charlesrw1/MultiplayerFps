#pragma once

#include "Render/Model.h"
#include "glm/glm.hpp"
#include "Animation.h"

#include "Framework/MemArena.h"
#include "Framework/InlineVec.h"
#include "Framework/EnumDefReflection.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Factory.h"
#include "Framework/PoolAllocator.h"
#include "Framework/ClassBase.h"
#include "Animation/SkeletonData.h"
#include "Game/SerializePtrHelpers.h"	// for AssetPtr
#include "Animation/AnimationSeqAsset.h"
#include <type_traits>
#include <cassert>

class Node_CFG;
struct MatrixPose
{
	glm::mat4 mats[256];
};

extern Pool_Allocator<Pose> g_pose_pool;
extern Pool_Allocator<MatrixPose> g_matrix_pool;

NEWENUM(rootmotion_setting, uint8_t)
{
	keep,
	remove,
	add_velocity
};


class BaseAGNode;
class Animation_Tree_CFG;
class Node_CFG;

