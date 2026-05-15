#pragma once
// Region tag for the nav baker. The owner's world transform defines the bake region:
// world AABB = (owner ws transform) applied to a unit cube spanning [-0.5, +0.5].
// Editor authors scale the entity directly via the gizmo — same convention as GiVolumeComponent.
// Editor-only: at start() in editor mode, spawns a cube1m mesh with a transparent zone material
// so the volume is visible.
//
// @docs [[navigation#volume-component]]

#include "Game/EntityComponent.h"

class MeshComponent;

class NavMeshVolumeComponent : public Component
{
public:
	CLASS_BODY(NavMeshVolumeComponent);
	NavMeshVolumeComponent() {
		set_call_init_in_editor(true);
#ifdef EDITOR_BUILD
		editor_is_editor_only = true;
#endif
	}

	void start() final;
	void stop() final;

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final { return "eng/editor/volume_icon.png"; }
#endif

private:
#ifdef EDITOR_BUILD
	MeshComponent* editor_mesh = nullptr;
#endif
};
