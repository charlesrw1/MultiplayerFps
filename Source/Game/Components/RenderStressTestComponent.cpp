#include "RenderStressTestComponent.h"
#include "Game/Entity.h"
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "Render/Model.h"
#include "Framework/Util.h"
#include "glm/gtc/matrix_transform.hpp"

#ifdef EDITOR_BUILD
#include "imgui.h"
#endif

void RenderStressTestComponent::start() {
	needs_rebuild = true;
	set_ticking(true);
	sync_render_data();
}
void RenderStressTestComponent::stop() {
	// safe to touch the render scene directly here: stop() runs at component teardown,
	// never inside the renderer's overlapped period (see MeshComponent::stop()).
	for (auto& h : instances)
		idraw->get_scene()->remove_obj(h);
	instances.clear();
	grid_offsets.clear();
}
void RenderStressTestComponent::update() {
	if (needs_rebuild || state == RenderStressTestState::EnabledAnimated)
		sync_render_data();
}

void RenderStressTestComponent::clear_grid() {
	for (auto& h : instances)
		idraw->get_scene()->remove_obj(h);
	instances.clear();
	grid_offsets.clear();
}

void RenderStressTestComponent::rebuild_grid() {
	clear_grid();
	Model* m = model.get();
	if (!m || grid_length <= 0)
		return;

	const int n = grid_length;
	instances.reserve((size_t)n * n);
	grid_offsets.reserve((size_t)n * n);

	const float half = (n - 1) * 0.5f * spacing;

	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			grid_offsets.emplace_back(i * spacing - half, j * spacing - half);
			instances.push_back(idraw->get_scene()->register_obj());
		}
	}
}

void RenderStressTestComponent::on_sync_render_data() {
	if (state == RenderStressTestState::Disabled) {
		if (needs_rebuild)
			clear_grid();
		needs_rebuild = false;
		return;
	}

	if (needs_rebuild) {
		rebuild_grid();
		needs_rebuild = false;
	}
	if (instances.empty())
		return;

	Model* m = model.get();
	const glm::vec3 center = get_ws_position();
	const float t = (state == RenderStressTestState::EnabledAnimated) ? (float)GetTime() * wave_speed : 0.f;

	Render_Object obj;
	obj.model = m;
	obj.visible = true;
	obj.shadow_caster = false;
	obj.ignore_in_baking = true;
	obj.ignore_in_cubemap = true;
	obj.owner = this;

	for (size_t idx = 0; idx < instances.size(); idx++) {
		const glm::vec2& offset = grid_offsets[idx];
		const float y = wave_height * sinf(wave_frequency * offset.x + t) * cosf(wave_frequency * offset.y + t);
		obj.transform = glm::translate(glm::mat4(1.f), center + glm::vec3(offset.x, y, offset.y));
		idraw->get_scene()->update_obj(instances[idx], obj);
	}
}

#ifdef EDITOR_BUILD
void RenderStressTestComponent::editor_on_change_property() {
	needs_rebuild = true;
	sync_render_data();
}

void RenderStressTestComponent::on_inspector_imgui() {
	ImGui::Text("Instances: %zu", instances.size());

	if (state == RenderStressTestState::Disabled) {
		if (ImGui::Button("Enable (Animated)")) {
			state = RenderStressTestState::EnabledAnimated;
			needs_rebuild = true;
			sync_render_data();
		}
		ImGui::SameLine();
		if (ImGui::Button("Enable (Static)")) {
			state = RenderStressTestState::EnabledStatic;
			needs_rebuild = true;
			sync_render_data();
		}
	}
	else {
		if (ImGui::Button("Disable")) {
			state = RenderStressTestState::Disabled;
			needs_rebuild = true;
			sync_render_data();
		}
		ImGui::SameLine();
		if (state == RenderStressTestState::EnabledAnimated) {
			if (ImGui::Button("Pause Animation"))
				state = RenderStressTestState::EnabledStatic;
		}
		else {
			if (ImGui::Button("Resume Animation")) {
				state = RenderStressTestState::EnabledAnimated;
				sync_render_data();
			}
		}
	}
}
#endif
