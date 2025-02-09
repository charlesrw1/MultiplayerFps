#pragma once

#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Animation/Runtime/Animation.h"
#include "Game/SerializePtrHelpers.h"
#include "Animation/AnimationSeqAsset.h"
#include "Game/Entities/CharacterController.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/EntityPtr.h"
#include "Physics/Physics2.h"
#include "Game/LevelAssets.h"
template<typename T>
inline T* class_cast(ClassBase* in) {
	return in ? in->cast_to<T>() : nullptr;
}
