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
#include "Framework/ArenaAllocator.h"
#include "Framework/ArenaStd.h"
#include "IGraphicsDevice.h"
#include "GpuCullingTest.h"
#include <bit>
#include <cstring>
// -----------------------------------------------------------------------
// BuildSceneData_CpuFast – LOD helpers, constructor, build_scene_data,
// shadow culling, and object-removal callbacks.
// -----------------------------------------------------------------------


inline int next_pow2(uint32_t x) {
	ASSERT(x > 0);

	return std::bit_ceil(x);
}

BuildSceneData_CpuFast::BuildSceneData_CpuFast() {
	ASSERT(gfx_is_initialized());

	// All bound via bind_storage_buffer_base in the GPU culling/draw passes;
	// cmd_list is also the source for multi_draw_elements_indirect.
	gpu.cmd_list = gfx().create_buffer({.flags = GraphicsBufferUseFlags(BUFFER_USE_AS_INDIRECT | BUFFER_USE_AS_STORAGE_READ)});
	gpu.cullobj_buf = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
	gpu.gbuffer_batches = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
	gpu.gbuffer_count = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
	gpu.gbuffer_draw_to_batch = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
	gpu.glinst_to_inst = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
	gpu.mod_data_gpu = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
	gpu.shadows_count = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
	gpu.shadow_batches = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
	gpu.shadow_draw_to_batch = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});

	// Compact instance path buffers: read by the CullComputeCompact dispatch AND
	// (via glinst indirection) the COMPACT_INST master-shader permutation.
	gpu.compact_inst_buf = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
	gpu.compact_prev_buf = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
	gpu.compact_desc_buf = gfx().create_buffer({.flags = BUFFER_USE_AS_STORAGE_READ});
}

void BuildSceneData_CpuFast::build_scene_data(bool cubemap_view, bool skybox_only) {
	CPU_SCOPE("BuildSceneData_CpuFast");
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
		CPU_SCOPE("rebuild_model");
		sys_print(Debug, "rebuilding fast path model data\n");
		rebuild_mod_data();
	}

	if (needs_new_model || wants_rebuild_counts) {
		CPU_SCOPE("rebuild_counts");

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
		CPU_SCOPE("bsd_fast_step2");

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

	build_compact_data();

	GpuCullingTest::inst->build_data(get_cull_input());
}

// ---------------------------------------------------------------------------
// Compact instance path (opt-in, GPU-driven)
// ---------------------------------------------------------------------------

void BuildSceneData_CpuFast::build_compact_data() {
	CPU_SCOPE("build_compact_data");

	// Descriptor table is indexed by batch_id == mod_data slot index (ptr_ofs), so
	// it is sized to mod_data_ptrs; non-compact slots stay zeroed (never read).
	// One entry per unique (model,material) combo -- tiny, so a plain vector.
	std::vector<gpu::CompactBatchDesc> descs(mod_data_ptrs.size());

	// Pass 1: live counts per region. Static-first ordering keeps the dynamic region's
	// base (== static_count) stable across frames where only dynamic live counts vary.
	int static_count = 0, dyn_count = 0;
	for (auto* ptr : mod_data_ptrs) {
		if (!ptr || !ptr->is_compact)
			continue;
		(ptr->compact_is_dynamic ? dyn_count : static_count) += ptr->instance_count;
	}
	const int total = static_count + dyn_count;
	num_compact_live = total;
	compact_static_count = static_count;

	const int elem = (int)sizeof(gpu::CompactInstance);

	// Grow-only allocation (high-water) so fluctuating live counts don't reallocate
	// the buffer -- and losing contents -- every frame. A realloc drops the static
	// contents, so re-send them this frame.
	const bool grew = total > compact_inst_capacity;
	if (grew) {
		compact_inst_capacity = next_pow2((uint32_t)total);
		gpu.compact_inst_buf->upload(nullptr, compact_inst_capacity * elem);
		compact_static_dirty = true;
	}

	const bool rebuild_static = compact_static_dirty && static_count > 0;
	if (rebuild_static)
		compact_static_dense.clear();
	compact_dyn_dense.clear();

	// Pass 2: descriptors + dense region fills. Static memcpy is skipped entirely
	// when the static region is clean (the compact path's per-frame CPU win).
	for (int i = 0; i < (int)mod_data_ptrs.size(); i++) {
		ModelAndMatTData* ptr = mod_data_ptrs[i];
		if (!ptr || !ptr->is_compact)
			continue;

		gpu::CompactBatchDesc d{};
		d.local_sphere = glm::vec4(ptr->local_bounds_center, ptr->local_bounds_radius);
		d.model_ofs = ptr->gpu_buf_ofs;
		d.mat_ofs = -1; // per-part material already baked into model_info for this slot
		descs[i] = d;

		const int live = ptr->instance_count;
		ASSERT(live <= (int)ptr->compact_staging.size());
		auto begin = ptr->compact_staging.begin();
		if (ptr->compact_is_dynamic) {
			ptr->compact_gpu_offset = static_count + (int)compact_dyn_dense.size();
			if (live > 0)
				compact_dyn_dense.insert(compact_dyn_dense.end(), begin, begin + live);
		} else {
			ptr->compact_gpu_offset = (int)(rebuild_static ? compact_static_dense.size() : 0);
			if (rebuild_static && live > 0)
				compact_static_dense.insert(compact_static_dense.end(), begin, begin + live);
		}
	}

	if (total == 0)
		return;

	gpu.compact_desc_buf->upload(descs.data(), (int)descs.size() * (int)sizeof(gpu::CompactBatchDesc));

	// Static region: sub-upload only when it actually changed.
	if (rebuild_static) {
		gpu.compact_inst_buf->sub_upload(compact_static_dense.data(), static_count * elem, 0);
		compact_static_dirty = false;
	}

	// Dynamic region: current frame into the unified buffer at the static_count base;
	// previous frame into the dyn-sized prev buffer (indexed by dyn-local ==
	// obj_index - static_count) for motion vectors. CPU arrays are swapped so this
	// frame's current becomes next frame's previous -- no copy, no double upload.
	if (dyn_count > 0) {
		gpu.compact_inst_buf->sub_upload(compact_dyn_dense.data(), dyn_count * elem, static_count * elem);

		if ((int)compact_dyn_dense_prev.size() != dyn_count)
			compact_dyn_dense_prev = compact_dyn_dense; // first frame / count change: prev = current
		if (dyn_count > compact_prev_capacity) {
			compact_prev_capacity = next_pow2((uint32_t)dyn_count);
			gpu.compact_prev_buf->upload(nullptr, compact_prev_capacity * elem);
		}
		gpu.compact_prev_buf->sub_upload(compact_dyn_dense_prev.data(), dyn_count * elem, 0);
		std::swap(compact_dyn_dense, compact_dyn_dense_prev);
	}
}

int16_t BuildSceneData_CpuFast::register_compact_batch(Model* m, MaterialInstance* mat, int capacity, bool is_dynamic) {
	if (!m)
		return -1;
	ASSERT(capacity > 0);

	// Reuse the existing model/material -> LOD/part/command-index slot resolution,
	// but in the compact slot namespace (is_compact=true) so it can never alias a
	// classic slot for the same (model,material) -- see ModelAndMatTextureSet.
	const int16_t id = get_index(m, mat, /*is_compact*/ true);
	ASSERT(id >= 0);
	ModelAndMatTData* ptr = mod_data_ptrs.at(id);

	// Compact slots are only ever touched here, so a fresh one has instance_alloced==0
	// and a re-registered one is already compact. (No classic proxy can reach this
	// slot now that the key is namespaced.)
	ASSERT(ptr->instance_alloced == 0 || ptr->is_compact);

	ptr->is_compact = true;
	ptr->compact_is_dynamic = is_dynamic;
	ptr->instance_alloced = capacity;
	ptr->instance_count = 0;
	ptr->compact_staging.assign((size_t)capacity, gpu::CompactInstance{});

	// Cache the model-space bounding sphere directly (NOT derived from a per-frame
	// scan) -- the GPU cull uses it to build each instance's world sphere on the fly.
	const glm::vec4 sphere = m->get_bounding_sphere();
	ptr->local_bounds_center = glm::vec3(sphere);
	ptr->local_bounds_radius = sphere.w;

	// Trigger the baseInstance-layout + mod_data rebuild so this slot's draw
	// commands get correct baseInstance ranges. Same path the classic scan would
	// eventually trigger; invoked directly instead of waiting for mmt_counts.
	force_rebuild = true;
	// Registration can add/remove a batch to/from the static set (including a
	// static<->dynamic switch on re-register), which shifts the static region -- so
	// always re-send it once. Cheap: registration is rare.
	compact_static_dirty = true;
	return id;
}

void BuildSceneData_CpuFast::resize_compact_batch(int16_t batch_id, int new_capacity) {
	ASSERT(is_compact_batch(batch_id));
	ASSERT(new_capacity > 0);
	ModelAndMatTData* ptr = mod_data_ptrs.at(batch_id);
	ptr->instance_alloced = new_capacity;
	ptr->compact_staging.resize((size_t)new_capacity);
	if (ptr->instance_count > new_capacity)
		ptr->instance_count = new_capacity;
	force_rebuild = true;
	compact_static_dirty = true; // may shift the static region layout
}

void BuildSceneData_CpuFast::set_instance_count(int16_t batch_id, int live_count) {
	ASSERT(is_compact_batch(batch_id));
	ModelAndMatTData* ptr = mod_data_ptrs.at(batch_id);
	ASSERT(live_count >= 0 && live_count <= ptr->instance_alloced);
	// A static live-count change shifts the dynamic region's base, so the static
	// region must be re-sent (only for static batches; dynamic re-uploads anyway).
	if (!ptr->compact_is_dynamic && ptr->instance_count != live_count)
		compact_static_dirty = true;
	ptr->instance_count = live_count;
}

void BuildSceneData_CpuFast::set_instances(int16_t batch_id, int dst_offset, std::span<const gpu::CompactInstance> src) {
	ASSERT(is_compact_batch(batch_id));
	ModelAndMatTData* ptr = mod_data_ptrs.at(batch_id);
	ASSERT(dst_offset >= 0 && dst_offset + (int)src.size() <= (int)ptr->compact_staging.size());
	if (!src.empty())
		memcpy(ptr->compact_staging.data() + dst_offset, src.data(), src.size() * sizeof(gpu::CompactInstance));
	if (!ptr->compact_is_dynamic)
		compact_static_dirty = true;
}

void BuildSceneData_CpuFast::set_instance(int16_t batch_id, int index, const gpu::CompactInstance& v) {
	set_instances(batch_id, index, std::span<const gpu::CompactInstance>(&v, 1));
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

	// Invalidate all proxy objects that referenced this model.
	// Set fastcpu_index to -1 (not in fast path) and clear proxy.model rather
	// than re-resolving — the model is gone and the pointer would dangle.
	auto& proxies = draw.scene.proxy_list.objects;
	for (auto& [_, obj] : proxies) {
		if (obj.proxy.model == m) {
			sys_print(Warning, "BuildSceneData: proxy still referencing removed model '%s', nulling fastcpu_index and proxy.model\n",
			          m->get_name().c_str());
			obj.proxy.model = nullptr;
			for (int idx : removed_indices) {
				if (obj.fastcpu_index == idx) {
					obj.fastcpu_index = -1;
					break;
				}
			}
		}
	}
}
