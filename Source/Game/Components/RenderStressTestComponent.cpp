#include "RenderStressTestComponent.h"
#include "Game/Entity.h"
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "Render/Model.h"
#include "Render/CompactInstancePack.h"
#include "Framework/Util.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "../Shaders/SharedGpuTypes.txt"

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
	// No unregister for compact batches; zero the live count so the (now orphaned)
	// slot stops drawing after teardown.
	if (compact_batch_id != kInvalidBatch)
		idraw->get_scene()->set_compact_instance_count(compact_batch_id, 0);
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

// Push the static grid through the compact instance path. Registers/resizes the
// batch on rebuild, then uploads all instances (identity rotation, unit scale,
// wave evaluated at t=0) and sets the live count.
void RenderStressTestComponent::on_sync_compact() {
	clear_grid(); // compact and classic grids are mutually exclusive

	Model* m = model.get();
	if (!m || grid_length <= 0) {
		if (compact_batch_id != kInvalidBatch)
			idraw->get_scene()->set_compact_instance_count(compact_batch_id, 0);
		return;
	}

	const int n = grid_length;
	const int count = n * n;
	const float half = (n - 1) * 0.5f * spacing;
	const glm::vec3 center = get_ws_position();

	if (needs_rebuild || compact_batch_id == kInvalidBatch)
		compact_batch_id = idraw->get_scene()->register_compact_batch(m, nullptr, count, /*is_dynamic*/ false);

	const uint32_t packed_rot = pack_quat_snorm8(glm::quat(1.f, 0.f, 0.f, 0.f)); // identity
	std::vector<gpu::CompactInstance> insts;
	insts.reserve((size_t)count);
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			const float x = i * spacing - half;
			const float z = j * spacing - half;
			const float y = wave_height * sinf(wave_frequency * x) * cosf(wave_frequency * z);
			gpu::CompactInstance ci{};
			ci.pos_x = center.x + x;
			ci.pos_y = center.y + y;
			ci.pos_z = center.z + z;
			ci.scale = 1.f;
			ci.packed_quat = packed_rot;
			ci.packed_batch_seed = pack_batch_seed(compact_batch_id, 0);
			insts.push_back(ci);
		}
	}
	idraw->get_scene()->set_compact_instances(compact_batch_id, 0, insts.data(), (int)insts.size());
	idraw->get_scene()->set_compact_instance_count(compact_batch_id, count);
}

void RenderStressTestComponent::on_sync_render_data() {
	if (state == RenderStressTestState::Disabled) {
		if (needs_rebuild) {
			clear_grid();
			if (compact_batch_id != kInvalidBatch)
				idraw->get_scene()->set_compact_instance_count(compact_batch_id, 0);
		}
		needs_rebuild = false;
		return;
	}

	if (state == RenderStressTestState::EnabledCompactStatic) {
		on_sync_compact();
		needs_rebuild = false;
		return;
	}

	// Classic Render_Object path; hide any compact grid we may have built.
	if (compact_batch_id != kInvalidBatch)
		idraw->get_scene()->set_compact_instance_count(compact_batch_id, 0);

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
		ImGui::SameLine();
		if (ImGui::Button("Enable (Compact Static)")) {
			state = RenderStressTestState::EnabledCompactStatic;
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
