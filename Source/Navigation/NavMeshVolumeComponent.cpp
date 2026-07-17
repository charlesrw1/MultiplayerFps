#include "NavMeshVolumeComponent.h"

#include "Game/Components/MeshComponent.h"
#include "Game/Entity.h"
#include "Render/Model.h"
#include "Render/MaterialPublic.h"
#include "GameEnginePublic.h"

void NavMeshVolumeComponent::start() {
	ASSERT(get_owner() != nullptr);
#ifdef EDITOR_BUILD
	if (eng->is_editor_level()) {
		editor_mesh = get_owner()->create_component<MeshComponent>();
		editor_mesh->set_invisible_to_bakes();
		
		editor_mesh->dont_serialize_or_edit = true;
		editor_mesh->set_model(Model::load("cube1m.cmdl"));
		editor_mesh->set_material_override(MaterialInstance::load("giprobe_zone.mi"));
	}
#endif
}

void NavMeshVolumeComponent::stop() {
#ifdef EDITOR_BUILD
	if (editor_mesh) {
		editor_mesh->destroy();
		editor_mesh = nullptr;
	}
#endif
}
