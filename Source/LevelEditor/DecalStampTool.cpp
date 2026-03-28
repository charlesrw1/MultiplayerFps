
#include "Game/Components/DecalComponent.h"
#include "EditorDocLocal.h"
#include "Render/MaterialPublic.h"

void DecalStampTool::tick(EditorInputs& inputs) {
	auto handle_scroll = [&]() {
		const bool hovered = UiSystem::inst->is_vp_hovered();
		const int amt = Input::get_mouse_scroll();
		if (hovered && !Input::is_mouse_down(1)) {
			const bool small = Input::is_shift_down();

			if (Input::is_ctrl_down()) {
				float scaleamt = small ? 0.05 : 0.25;

				scale -= (scale * scaleamt) * amt;
				if (abs(scale) < 0.000001)
					scale = 0.0001;
			}
			else {
				float scaleamt = small ? (TWOPI / 100) : (TWOPI / 15);

				rotation += amt * scaleamt;
			}
		}
	};
	handle_scroll();

	Entity* e = preview.get();
	if (!e) {
		e = doc.spawn_entity();
		e->dont_serialize_or_edit = true;
		auto mesh = e->create_component<DecalComponent>();
		preview = e;
	}
	e->set_hidden_in_editor(true);
	auto decal_comp = e->get_component<DecalComponent>();
	auto& selected_resource = AssetBrowser::inst->selected_resource;
	MaterialInstance* material{};
	if (selected_resource.type && selected_resource.type->get_asset_class_type() == &MaterialInstance::StaticType) {
		material = MaterialInstance::load(selected_resource.filename);
		if (material)
			decal_comp->set_material(material);
	}

	const bool is_hovered = UiSystem::inst->is_vp_hovered();

	if (!is_hovered || !inputs.can_use_mouse_click())
		return;

	const auto mouse = Input::get_mouse_pos();
	glm::vec3 dir = doc.unproject_mouse_to_ray(mouse.x, mouse.y).dir;
	glm::vec3 pos = doc.get_vs()->origin;
	world_query_result res;
	const bool had_hit = g_physics.trace_ray(res, pos, pos - dir * 100.f, nullptr, UINT32_MAX);

	if (!had_hit) {
		return;
	}
	e->set_hidden_in_editor(false);
	e->set_ws_position(res.hit_pos);

	glm::vec3 place_pos = res.hit_pos;
	glm::vec3 N = res.hit_normal;

	glm::vec3 refUp = (fabs(N.z) < 0.999f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);

	glm::vec3 T = glm::normalize(glm::cross(refUp, N)); // tangent
	glm::vec3 B = glm::cross(N, T);						// bitangent
	const float theta = rotation;
	auto T2 = T * cos(theta) + B * sin(theta);
	auto B2 = -T * sin(theta) + B * cos(theta);
	glm::mat4 rotation_matrix(1);
	rotation_matrix[0] = glm::vec4(T2, 0);
	rotation_matrix[1] = glm::vec4(B2, 0);
	rotation_matrix[2] = glm::vec4(N, 0);
	auto mat = glm::translate(glm::mat4(1), res.hit_pos) * rotation_matrix;
	mat = glm::scale(mat, glm::vec3(scale, scale, 0.3));
	e->set_ws_transform(mat);

	if (Input::was_mouse_pressed(0) && material) {
		// create new entity
		doc.command_mgr->add_command(new CreateEntityCommand(doc, [mat, material](Entity* ent) {
			ent->set_ws_transform(mat);
			auto decal = ent->create_component<DecalComponent>();
			decal->set_material(material);
			}));
	}
}

