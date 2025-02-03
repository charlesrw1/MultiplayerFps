#pragma once
#include "Animation/Runtime/Animation.h"
#include "Game/EntityComponent.h"
#include "Game/Entity.h"
#include "Sound/SoundPublic.h"
#include "Game/SerializePtrHelpers.h"
#include "Game/Components/MeshComponent.h"
#include "Animation/AnimationSeqAsset.h"

NEWCLASS(TDChest, EntityComponent)
public:
	REFLECT();
	AssetPtr<SoundFile> soundfx;
	REFLECT();
	AssetPtr<AnimationSeqAsset> openanim;

	MeshComponent* m = nullptr;
	void start() {
		m = get_owner()->get_component<MeshComponent>();
	}

	REFLECT();
	void open_chest() {
		if (!is_open) {

			isound->play_sound(
				soundfx, 1, 1, 1, 1, {}, false, false, {}
			);

			m->get_animator_instance()->play_animation_in_slot(
				openanim->seq,
				StringName("Default"),
				1, 0
			);

			is_open = true;
		}
		else {
			is_open = false;
		}
	}

	bool is_open = false;
};

NEWCLASS(ChestAnimator, AnimatorInstance)
public:

	TDChest* chest = nullptr;

	void on_init() override {
		chest = get_owner()->get_component<TDChest>();
	}
	void on_update(float dt) override {
		this->chest_open = chest->is_open;
	}

	REFLECT();
	bool chest_open = false;
};