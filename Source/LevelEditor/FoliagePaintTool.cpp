#include "EditorModes.h"
#include "EditorDocLocal.h"
int SamplePoisson(float lambda, Random& r) {
	float L = exp(-lambda);
	int k = 0;
	float p = 1.0f;

	do {
		k++;
		p *= r.RandF(0, 1);
	} while (p > L);

	return k - 1;
}
ConfigVar foliage_density("foliage_density", "3", CVAR_FLOAT | CVAR_UNBOUNDED, "");
ConfigVar foliage_brush("foliage_brush", "1", CVAR_FLOAT | CVAR_UNBOUNDED, "");

ConfigVar foliage_exdensity("foliage_exdensity", "0.7", CVAR_FLOAT | CVAR_UNBOUNDED, "");

void FoliagePaintTool::tick(EditorInputs& inputs) {
	if (ImGui::Begin("Foliage")) {

		static char buffer[1000];
		ImGui::InputTextMultiline("#box", buffer, 1000, ImVec2(0, 0));
	}
	ImGui::End();

	Entity* e = orb_cursor.get();
	if (!e) {
		e = doc.spawn_entity();
		e->dont_serialize_or_edit = true;
		auto mesh = e->create_component<MeshComponent>();
		mesh->set_model_str("sphere.cmdl");
		mesh->set_material_override(MaterialInstance::load("orb_cursor.mm"));
		orb_cursor = e;
	}
	e->set_hidden_in_editor(true);

	const float dt = 1.0 / 60.0;
	const float density = foliage_density.get_float();
	float R = foliage_brush.get_float();
	const float area = PI * (R * R);
	const float exclusive_radius = foliage_exdensity.get_float();
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
	e->set_ls_scale(glm::vec3(R));

	const bool wants_delete = Input::is_key_down(SDL_SCANCODE_LCTRL);

	if (Input::is_mouse_down(0)) {
		float rate = density * area;
		float lambda = rate * dt;
		int toPlace = SamplePoisson(lambda, ran);

		if (wants_delete) {
			bool wants_continue = true;
			std::vector<int> indicies;
			for (int j = 0; j < foliage.size(); j++) {
				glm::vec3 dist = res.hit_pos - foliage[j].pos;
				float dist2 = dot(dist, dist);
				if (dist2 <= R * R) {
					indicies.push_back(j);
				}
			}

			if (indicies.empty())
				return;

			int count = std::min(toPlace, (int)indicies.size());

			// Partial Fisher�Yates shuffle
			for (int i = 0; i < count; i++) {
				int r = ran.RandI(i, (int)indicies.size() - 1);
				std::swap(indicies[i], indicies[r]);
			}

			// First `count` are unique, random
			std::vector<int> remove_these;
			remove_these.reserve(count);

			for (int i = 0; i < count; i++) {
				remove_these.push_back(indicies[i]);
			}

			std::sort(remove_these.rbegin(), remove_these.rend());

			for (int i = 0; i < remove_these.size(); i++) {
				auto b = foliage[remove_these[i]];
				idraw->get_scene()->remove_obj(b.object);

				foliage.erase(foliage.begin() + remove_these[i]);
			}

		}
		else {
			for (int i = 0; i < toPlace; i++) {
				// place instance
				float theta = ran.RandF(0, 2 * PI);
				float r = R * sqrt(ran.RandF(0, 1));
				float x = cos(theta) * r;
				float y = sin(theta) * r;

				// get placement

				glm::vec3 place_pos = res.hit_pos + glm::vec3(x, 0, y);
				glm::mat4 rotation_matrix = glm::mat4(1);
				// get the y component from raycast

				{
					bool hit_surface = g_physics.trace_ray(res, place_pos + glm::vec3(0, 1, 0),
						place_pos - glm::vec3(0, 1, 0), nullptr, UINT32_MAX);
					if (!hit_surface) {
						continue;
					}
					place_pos = res.hit_pos;
					glm::vec3 N = res.hit_normal;

					glm::vec3 refUp = (fabs(N.z) < 0.999f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);

					glm::vec3 T = glm::normalize(glm::cross(refUp, N)); // tangent
					glm::vec3 B = glm::cross(N, T);						// bitangent
					auto T2 = T * cos(theta) + B * sin(theta);
					auto B2 = -T * sin(theta) + B * cos(theta);
					rotation_matrix[0] = glm::vec4(B2, 0);
					rotation_matrix[1] = glm::vec4(N, 0);
					rotation_matrix[2] = glm::vec4(T2, 0);
				}

				bool wants_continue = true;
				for (int j = 0; j < foliage.size(); j++) {
					glm::vec3 dist = place_pos - foliage[j].pos;
					float dist2 = dot(dist, dist);
					if (dist2 <= exclusive_radius * exclusive_radius) {
						wants_continue = false;
						break;
					}
				}
				if (wants_continue) {

					Render_Object ro;
					ro.model = Model::load("grass_low.cmdl");
					ro.transform = glm::translate(glm::mat4(1), place_pos);
					ro.transform = glm::scale(ro.transform * rotation_matrix, glm::vec3(1));
					auto handle = idraw->get_scene()->register_obj();

					idraw->get_scene()->update_obj(handle, ro);

					foliage.push_back({ handle, place_pos });
				}
			}
		}
	}
}
