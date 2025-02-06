#pragma once
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Animation/Runtime/Animation.h"
#include "Game/SerializePtrHelpers.h"
#include "Animation/AnimationSeqAsset.h"
#include "Game/Entities/CharacterController.h"

NEWCLASS(TPPlayerAnimator, AnimatorInstance)
public:

	REFLECT();
	bool bRunning = false;
	REFLECT();
	float flLean = 0.0;
};
NEWCLASS(TPPlayer, EntityComponent)
public:
	REFLECT();
	AssetPtr<AnimationSeqAsset> jump_seq;
	REFLECT();
	AssetPtr<AnimationSeqAsset> idle_to_run_seq;
	REFLECT();
	AssetPtr<AnimationSeqAsset> run_to_idle_seq;

	void start() override {

	}

	void update() override {

	}

	void end() override {

	}
};