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

			m->get_animator()->play_animation(openanim);

			is_open = true;
		}
		else {
			is_open = false;
		}
	}

	bool is_open = false;
};

