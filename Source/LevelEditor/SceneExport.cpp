
#ifdef EDITOR_BUILD
#include "cgltf_write.h"
#include <vector>
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Render/Model.h"
#include "GameEnginePublic.h"
void export_scene_model()
{
	cgltf_data data{};
	cgltf_node main_node{};
	cgltf_mesh main_mesh{};
	cgltf_primitive main_prim{};
	cgltf_accessor pos_accessor{};
	cgltf_accessor texcoord_accessor{};
	cgltf_accessor normal_accessor{};
	cgltf_buffer_view buf_view{};
	cgltf_buffer main_buf{};
	std::vector<char> buf_data;

	main_node.mesh = &main_mesh;
	main_mesh.primitives_count = 1;
	main_mesh.primitives = &main_prim;

	auto level = eng->get_level();
	for (auto node : level->get_all_objects()) {
		if (!node->is_a<MeshComponent>()) continue;
		auto mc = node->cast_to<MeshComponent>();
		if (!mc->get_is_visible()) continue;
		if (mc->dont_serialize_or_edit) continue;
		if (mc->get_owner()->get_hidden_in_editor()) continue;
		auto model = mc->get_model();
		if (!model) continue;
		if (model->get_num_lods() == 0) continue;
		auto raw_data = model->get_raw_mesh_data();
		auto& lod = model->get_lod(0);

	}
}

#endif