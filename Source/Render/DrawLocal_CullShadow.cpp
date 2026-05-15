#include "DrawLocal.h"
#include "Framework/Util.h"
#include "glad/glad.h"
#include "Render/Texture.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Debug.h"
#include "Assets/AssetDatabase.h"
#include "Render/ModelManager.h"
#include "tracy/public/tracy/Tracy.hpp"
#include "Framework/ArenaAllocator.h"
#include "Framework/ArenaStd.h"
#include "IGraphsDevice.h"
#include "GpuCullingTest.h"
#include <bit>
// -----------------------------------------------------------------------
// BuildSceneData_CpuFast – LOD helpers, constructor, build_scene_data,
// shadow culling, and object-removal callbacks.
// -----------------------------------------------------------------------


inline int next_pow2(uint32_t x) {
	ASSERT(x > 0);

	return std::bit_ceil(x);
}

BuildSceneData_CpuFast::BuildSceneData_CpuFast() {
	ASSERT(IGraphicsDevice::inst != nullptr);

	gpu.cmd_list = IGraphicsDevice::inst->create_buffer({});
	gpu.cullobj_buf = IGraphicsDevice::inst->create_buffer({});
	gpu.gbuffer_batches = IGraphicsDevice::inst->create_buffer({});
	gpu.gbuffer_count = IGraphicsDevice::inst->create_buffer({});
	gpu.gbuffer_draw_to_batch = IGraphicsDevice::inst->create_buffer({});
	gpu.glinst_to_inst = IGraphicsDevice::inst->create_buffer({});
	gpu.mod_data_gpu = IGraphicsDevice::inst->create_buffer({});
	gpu.shadows_count = IGraphicsDevice::inst->create_buffer({});
	gpu.shadow_batches = IGraphicsDevice::inst->create_buffer({});
	gpu.shadow_draw_to_batch = IGraphicsDevice::inst->create_buffer({});
}

void BuildSceneData_CpuFast::build_scene_data(bool cubemap_view, bool skybox_only) {
	ZoneScopedN("BuildSceneData_CpuFast");
	CPUFUNCTIONSTART;
	ASSERT(BuildSceneData_CpuFast::inst != nullptr);

	auto& arena = draw.get_arena();
	auto& proxies = draw.scene.proxy_list.objects;

	ArenaScope scope(arena);

	// step 1.1 — count instances per fast-path slot
	auto mmt_counts = arena.alloc_bottom_span<uint16>(mod_data_ptrs.size());
	std::fill(mmt_counts.begin(), mmt_counts.end(), 0);
	for (auto& [_, obj] : proxies) {
		if (obj.fastcpu_index >= 0)
			mmt_counts[obj.fastcpu_index] += 1;
	}

	// step 1.2 — decide whether a rebuild is needed
	const int thresh = 1;
	bool wants_rebuild_counts = false;
	bool needs_new_model = force_rebuild;
	for (int c = 0; c < (int)mmt_counts.size(); c++) {
		const int count = mmt_counts[c];
		auto ptr = mod_data_ptrs.at(c);
		if (count >= thresh) {
			ptr->instance_count = count;
			if (count > ptr->instance_alloced) {
				if (ptr->instance_alloced == 0) {
					needs_new_model = true;
				} else
					wants_rebuild_counts = true;

				ptr->instance_alloced = next_pow2(count);
			}

			if (count > 0 && !ptr->m->is_valid_to_use()) {
				sys_print(Debug, "emergency model reload %s\n", ptr->m->get_name().c_str());
				g_assets.reload<Model>(ptr->m);
			}
		}
	}
	force_rebuild = false;

	// step 1.3 — expensive rebuild if a new model/material combo appeared
	if (needs_new_model) {
		ZoneScopedN("rebuild_model");
		sys_print(Debug, "rebuilding fast path model data\n");
		rebuild_mod_data();
	}

	if (needs_new_model || wants_rebuild_counts) {
		ZoneScopedN("rebuild_counts");

		int count_sum = 0;
		for (int cmdi = 0; cmdi < (int)out_cmds.size(); cmdi++) {
			auto ptr = mod_data_ptrs.at(cmd_to_mod_data_ptr.at(cmdi));
			auto& cmd = out_cmds[cmdi];
			cmd.baseInstance = count_sum;
			count_sum += ptr->instance_alloced;
		}

		gbuffer_list.glinstances.resize(count_sum);

		upload_gpu_cmds(count_sum);
	}

	// step 2 — populate per-frame cull object list
	{
		ZoneScopedN("bsd_fast_step2");

		for (int i = 0; i < (int)out_cmds.size(); i++)
			out_cmds[i].primCount = 0;

		if (skybox_only)
			return; // no opaque objects in this pass

		arena_vec<CullObject> cull_obj_gpu_buf(scope);
		cull_obj_gpu_buf.reserve(proxies.size());

		int index = -1;
		for (auto& [_, obj] : proxies) {
			index += 1;

			const int fast_idx = obj.fastcpu_index;
			const bool wants_skip = (fast_idx < 0) || (!obj.proxy.visible) || (obj.proxy.is_skybox) ||
									(cubemap_view && obj.proxy.ignore_in_cubemap);

			if (wants_skip)
				continue;
			ModelAndMatTData* ptr = mod_data_ptrs.at(fast_idx);
			if (ptr->instance_alloced > 0) {
				CullObject co;
				co.bounds_sphere = obj.bounding_sphere_and_radius;
				const int mat_ofs = (obj.proxy.mat_override && obj.proxy.mat_override->impl)
										? (obj.proxy.mat_override->impl->gpu_buffer_offset)
										: -1;
				co.model_ofs = glm::ivec4(ptr->gpu_buf_ofs, index, mat_ofs, 0);
				if (obj.proxy.shadow_caster)
					co.model_ofs.w |= 1;

				cull_obj_gpu_buf.push_back(co);
			}
		}
		gpu.cullobj_buf->upload(cull_obj_gpu_buf.data(), (int)cull_obj_gpu_buf.size() * sizeof(CullObject));
		gpu.num_cullobjs = (int)cull_obj_gpu_buf.size();
	}

	GpuCullingTest::inst->build_data(get_cull_input());
}

// Free function wrappers used by the shadow system.
void cull_and_draw_cascade_fucker(int idx) {
	ASSERT(BuildSceneData_CpuFast::inst != nullptr);
	BuildSceneData_CpuFast::inst->cull_and_draw_shadow_cascade(idx);
}
void cull_and_draw_spot(Frustum f) {
	ASSERT(BuildSceneData_CpuFast::inst != nullptr);
	BuildSceneData_CpuFast::inst->cull_and_draw_shadow_spot(f);
}

extern void build_frustum_for_cascade(Frustum& f, int index);

void BuildSceneData_CpuFast::cull_and_draw_shadow_cascade(int idx) {
	ASSERT(idx >= 0);

	Frustum f;
	build_frustum_for_cascade(f, idx);
	ASSERT(f.is_ortho);
	GpuCullingTest::inst->do_shadow_cull(get_cull_input_shadow(), f);
	do_shadow_draw(1.0, true);
}

void BuildSceneData_CpuFast::cull_and_draw_shadow_spot(const Frustum& f) {
	ASSERT(GpuCullingTest::inst != nullptr);

	GpuCullingTest::inst->do_shadow_cull(get_cull_input_shadow(), f);
	do_shadow_draw(-3, false);
}

void BuildSceneData_CpuFast::make_shadow_object_data_threadsafe(std::span<uint8_t> vis, std::span<int> glinst,
																std::span<gpu::DrawElementsIndirectCommand> outcmds,
																std::span<int> mdcounts) const {
	ASSERT(glinst.size() == gbuffer_list.glinstances.size());
	ASSERT(outcmds.size() == out_cmds.size());
	ASSERT(mdcounts.size() == shadow_pass.batches.size());

	for (int i = 0; i < (int)outcmds.size(); i++) {
		outcmds[i] = out_cmds[i];
		outcmds[i].primCount = 0;
	}

	const auto& proxies = draw.scene.proxy_list.objects;
	int index = -1;
	for (auto& [_, obj] : proxies) {
		index += 1;

		const int fast_idx = obj.fastcpu_index;
		const bool wants_skip = (fast_idx < 0) || (!obj.proxy.visible) || (!obj.proxy.shadow_caster);

		if (wants_skip)
			continue;
		ModelAndMatTData* ptr = mod_data_ptrs.at(fast_idx);
		if (ptr->instance_alloced > 0) {

			bool visible{};
			int8_t wantlod{};
			split_input_lod_arr(vis[index], visible, wantlod);
			if (visible) {
				auto& lod = obj.proxy.model->get_lod(wantlod);
				for (int part = 0; part < lod.part_count; part++) {
					const int part_i = lod.part_ofs + part;
					const int drawcmd_i = ptr->part_to_draw_cmd.at(part_i * 2);
					const int prim = out_cmds.at(drawcmd_i).primCount;
					const int base = out_cmds.at(drawcmd_i).baseInstance;
					outcmds[drawcmd_i].primCount += 1;
					glinst[base + prim] = index;
				}
			}
		}
	}

	index = -1;
	for (auto& md : shadow_pass.batches) {
		index += 1;
		int start = 0;
		const int count = md.count;
		int actual_count = 0;
		for (int i = 0; i < count; i++) {
			auto& cmd = outcmds[md.first + i];
			if (cmd.primCount > 0) {
				outcmds[md.first + start] = cmd;
				start += 1;
				actual_count += 1;
			}
		}
		mdcounts[index] = actual_count;
	}
}

void BuildSceneData_CpuFast::on_fastpath_material_removed(MaterialInstance* mat) {
	ASSERT(mat != nullptr);
	assert(mat->impl->used_in_fastpath_cache);
	sys_print(Warning, "on_fastpath_material_removed\n");

	auto invalidate_these = [&](int fast_index) {
		for (auto& [_, obj] : draw.scene.proxy_list.objects) {
			if (obj.fastcpu_index == fast_index) {
				obj.fastcpu_index = get_index(obj.proxy.model, obj.proxy.mat_override);
			}
		}
	};

	opt<ModelAndMatTextureSet> found_key;
	int fast_index = -1;
	for (auto& [key, data] : mod_data) {
		if (key.has_textures == mat) {
			found_key = key;
			fast_index = data.ptr_ofs;
			break;
		}
	}
	if (found_key.has_value()) {
		mod_data_ptrs.at(fast_index) = nullptr;
		mod_data.erase(found_key.value());
		invalidate_these(fast_index);
	} else {
		sys_print(Warning, "on_fastpath_material_removed: couldn't find material???\n");
	}
}

void BuildSceneData_CpuFast::on_model_removed(Model* m) {
	ASSERT(m != nullptr);

	// Collect all fast-path cache entries keyed on this model (one per material combo).
	std::vector<int> removed_indices;
	std::vector<ModelAndMatTextureSet> keys_to_erase;
	for (auto& [key, data] : mod_data) {
		if (key.m == m) {
			removed_indices.push_back(data.ptr_ofs);
			keys_to_erase.push_back(key);
		}
	}
	if (removed_indices.empty())
		return;

	sys_print(Debug, "BuildSceneData: nulling %d fast-path entries for removed model\n",
	          (int)removed_indices.size());

	for (int idx : removed_indices)
		mod_data_ptrs.at(idx) = nullptr;
	for (const auto& key : keys_to_erase)
		mod_data.erase(key);

	// Invalidate all proxy objects that referenced any of those cache slots.
	// Set to -1 (not in fast path) rather than re-resolving — the model is gone.
	auto& proxies = draw.scene.proxy_list.objects;
	for (auto& [_, obj] : proxies) {
		for (int idx : removed_indices) {
			if (obj.fastcpu_index == idx) {
				sys_print(Warning, "BuildSceneData: proxy still referencing removed model '%s', nulling fastcpu_index\n",
				          m->get_name().c_str());
				obj.fastcpu_index = -1;
				break;
			}
		}
	}
}
