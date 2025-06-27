#pragma once
#include "Animation/Runtime/Animation.h"
#include "Game/EntityComponent.h"
#include "Game/Entity.h"
#include "Sound/SoundPublic.h"
#include "Game/SerializePtrHelpers.h"
#include "Game/Components/MeshComponent.h"
#include "Animation/AnimationSeqAsset.h"
#include "Game/SoftAssetPtr.h"
class TDChest : public Component
{
public:
	CLASS_BODY(TDChest);
	
	REF AssetPtr<SoundFile> soundfx;
	REF AssetPtr<AnimationSeqAsset> openanim;
	REF SoftAssetPtr<AnimationSeqAsset> delayedLoadAnim;

	MeshComponent* m = nullptr;
	void start() {
		m = get_owner()->get_component<MeshComponent>();
	}

	REF void open_chest() {
		if (!is_open) {

			isound->play_sound(
				soundfx, 1, 1, 1, 1, {}, false, false, {}
			);

			m->get_animator()->play_animation_in_slot(
				openanim,
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


class ChestAnimator : public AnimatorInstance
{
public:
	CLASS_BODY(ChestAnimator);

	TDChest* chest = nullptr;

	void on_init() override {
		chest = get_owner()->get_component<TDChest>();
	}
	void on_update(float dt) override {
		if (chest) {
			this->chest_open = chest->is_open;
		}
	}

	REF bool chest_open = false;
};