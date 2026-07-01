// Source/IntegrationTests/Tests/Assets/test_asset_system.cpp
//
// Asset system invariants from .claude/plans/review-assetdatabase-and-iasset-pure-pebble.md.
//
//   1. Load once, keep forever — addresses are stable across reload.
//   2. Failed loads survive serialization round-trip — tombstones in the
//      AssetDatabase preserve the requested path byte-for-byte and report
//      did_load_fail() == true forever (until reload succeeds).
//   3. find<T> for a missing path returns a null AssetPtr<T>; generic_find
//      exposes the tombstone via get_unsafe() so serializers can round-trip.
//   4. reload(asset) runs uninstall + load_asset + post_load on the same
//      instance.  Anyone holding a raw IAsset* keeps a valid pointer.
//   5. MaterialInstance master-material reload cascades to dependent
//      instances.  A failed master reload leaves the children inert
//      (their masterMaterial pointer still equals the master, but the
//      master tombstones) — no crash on access.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Assets/AssetDatabase.h"
#include "Render/Texture.h"
#include "Render/Model.h"
#include "Render/MaterialPublic.h"
#include "Framework/Files.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/Runtime/RuntimeNodesNew2.h"
#include "Animation/AnimationSeqAsset.h"
#include "Animation/SkeletonData.h"
#include "Animation/SkeletonData.h"

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

static bool write_game_file(const std::string& path, const std::string& content) {
	auto f = FileSys::open_write_game(path);
	if (!f)
		return false;
	f->write(content.data(), content.size());
	f->close();
	return true;
}

struct ScopedTempFile {
	std::string path;
	explicit ScopedTempFile(std::string p) : path(std::move(p)) {}
	~ScopedTempFile() { FileSys::delete_game_file(path); }
	ScopedTempFile(const ScopedTempFile&) = delete;
	ScopedTempFile& operator=(const ScopedTempFile&) = delete;
};

// ---------------------------------------------------------------------------
// Test 1: failed-load preserves path byte-for-byte
// ---------------------------------------------------------------------------
// A missing texture path must produce a tombstone whose get_name() equals
// the original path verbatim — this is what guarantees a broken map
// reference survives load+save round-trip without silent rewriting.

static TestTask test_failed_load_path_preserved(TestContext& t) {
	const std::string missing = "eng/__definitely_does_not_exist_xyz.dds";
	auto a = g_assets.generic_find(missing, &Texture::StaticType);
	t.require(a.get_unsafe() != nullptr, "tombstone instance exists in map");
	t.check(a.get_unsafe()->get_name() == missing, "tombstone preserves original path");
	t.check(a.get_unsafe()->did_load_fail(), "tombstone reports did_load_fail()");
	t.check(a.get_unsafe()->was_load_attempted(), "tombstone reports was_load_attempted()");
	t.check(!a.get_unsafe()->is_valid_to_use(), "tombstone not is_valid_to_use()");
	co_return;
}
GAME_TEST("assets/failed_load_path_preserved", 5.f, test_failed_load_path_preserved);

// ---------------------------------------------------------------------------
// Test 2: tombstone stable across repeated find calls
// ---------------------------------------------------------------------------
// Once a load fails, subsequent find<T>(same_path) must return the same
// tombstone — no retry storm, no duplicate entries.

static TestTask test_failed_load_tombstone_stable(TestContext& t) {
	const std::string missing = "eng/__tombstone_test_xyz.dds";
	auto first = g_assets.generic_find(missing, &Texture::StaticType);
	auto second = g_assets.generic_find(missing, &Texture::StaticType);
	t.require(first.get_unsafe() != nullptr, "first lookup returned tombstone");
	t.require(second.get_unsafe() != nullptr, "second lookup returned tombstone");
	t.check(first.get_unsafe() == second.get_unsafe(), "tombstone instance shared across calls");
	t.check(g_assets.is_asset_loaded(missing), "tombstone occupies the map slot");
	co_return;
}
GAME_TEST("assets/failed_load_tombstone_stable", 5.f, test_failed_load_tombstone_stable);

// ---------------------------------------------------------------------------
// Test 3: typed find<T> returns null for a missing path
// ---------------------------------------------------------------------------
// AssetPtr<T>::operator bool gates on !did_load_fail().  Gameplay code that
// writes `if (auto m = g_assets.find<Texture>(path))` must see false for a
// missing/broken asset and skip the branch naturally.

static TestTask test_find_missing_returns_null(TestContext& t) {
	const std::string missing = "eng/__find_missing_test_xyz.dds";
	auto p = g_assets.find<Texture>(missing);
	t.check(!p, "find<Texture>(missing) is operator-bool false");
	t.check(p.get() == nullptr, "find<Texture>(missing).get() is nullptr");
	t.check(!p.is_valid(), "is_valid() is false");
	co_return;
}
GAME_TEST("assets/find_missing_returns_null", 5.f, test_find_missing_returns_null);

// ---------------------------------------------------------------------------
// Test 4: generic_find exposes the tombstone for the serializer path
// ---------------------------------------------------------------------------
// SerializerJson reads an IAsset* via generic_find(...).get_unsafe() and
// writes asset->get_name() to disk.  The tombstone must be reachable via
// get_unsafe() so the original path round-trips even when the asset is
// missing on disk.

static TestTask test_find_missing_unsafe_returns_tombstone(TestContext& t) {
	const std::string missing = "eng/__unsafe_tombstone_test_xyz.dds";
	auto a = g_assets.generic_find(missing, &Texture::StaticType);
	IAsset* tomb = a.get_unsafe();
	t.require(tomb != nullptr, "get_unsafe returns tombstone, not nullptr");
	t.check(tomb->get_name() == missing, "name survives round-trip");
	t.check(tomb->did_load_fail(), "marked failed");
	co_return;
}
GAME_TEST("assets/find_missing_unsafe_returns_tombstone", 5.f, test_find_missing_unsafe_returns_tombstone);

// ---------------------------------------------------------------------------
// Test 5: Texture reload preserves the IAsset* address
// ---------------------------------------------------------------------------
// Stable addresses are the load-bearing invariant of the in-place reload
// design.  Any system caching `Texture*` after the first load must see the
// same pointer after reload — only the data inside changes.

static TestTask test_texture_reload_pointer_stable(TestContext& t) {
	const std::string path = "eng/5x5grid.dds";
	auto first = g_assets.find<Texture>(path);
	t.require(first.is_valid(), "5x5grid.dds loaded for reload-stability test");
	Texture* before = first.get();

	g_assets.reload(first);

	auto second = g_assets.find<Texture>(path);
	t.require(second.is_valid(), "texture still valid after reload");
	t.check(second.get() == before, "Texture* identical before and after reload");
	co_return;
}
GAME_TEST("assets/texture_reload_pointer_stable", 10.f, test_texture_reload_pointer_stable);

// ---------------------------------------------------------------------------
// Test 6: Model reload preserves the IAsset* address AND the MSkeleton address
// ---------------------------------------------------------------------------
// Components hold model->get_skel() pointers; the plan promises that those
// remain stable across reload because Model::uninstall preserves the
// unique_ptr<MSkeleton> and only wipes its contents.

static TestTask test_model_reload_pointer_stable(TestContext& t) {
	const std::string path = "eng/cube.cmdl";
	auto first = g_assets.find<Model>(path);
	t.require(first.is_valid(), "eng/cube.cmdl loaded for reload-stability test");
	Model* before = first.get();
	const void* skel_before = static_cast<const void*>(first->get_skel());

	g_assets.reload(first);

	auto second = g_assets.find<Model>(path);
	t.require(second.is_valid(), "model still valid after reload");
	t.check(second.get() == before, "Model* identical before and after reload");
	t.check(static_cast<const void*>(second->get_skel()) == skel_before,
			"get_skel() address identical before and after reload");
	co_return;
}
GAME_TEST("assets/model_reload_pointer_stable", 10.f, test_model_reload_pointer_stable);

// ---------------------------------------------------------------------------
// Test 7: MaterialInstance::load_asset ASSERT(!impl) precondition holds
// ---------------------------------------------------------------------------
// New design: reload always runs uninstall() (which resets impl to null)
// before load_asset().  If anything bypasses that sequence and load_asset()
// runs with non-null impl, the assertion in MaterialInstance::load_asset
// fires.  Reloading twice in succession exercises that path.

static TestTask test_material_reload_precondition(TestContext& t) {
	eng->load_level("");
	const std::string path = "mats/test_reload_precondition.mi";
	ScopedTempFile guard(path);
	t.require(write_game_file(path,
							  "TYPE MaterialInstance\nPARENT defaultPBR.mm\nVAR colorMult 200 100 50 255\n"),
			  "wrote .mi fixture");

	auto mat = g_assets.find<MaterialInstance>(path);
	t.require(mat.is_valid(), "fixture .mi loaded");

	// Two reloads back-to-back: the second's load_asset() must see impl == nullptr
	// because the second uninstall() ran first.  Hitting the ASSERT would crash.
	g_assets.reload(mat);
	co_await t.wait_ticks(1);
	g_assets.reload(mat);
	co_await t.wait_ticks(1);
	t.check(mat.is_valid(), "material still valid after two reloads");
}
GAME_TEST("assets/material_reload_precondition", 15.f, test_material_reload_precondition);

// ---------------------------------------------------------------------------
// Test 8: master-material reload cascades to dependent instances
// ---------------------------------------------------------------------------
// When a .mm is reloaded, every loaded .mi whose master is that .mm must
// have post_load() run again (so the children pick up new shader uniforms,
// texture parameter rebinds, etc.).  Detect via on_material_loaded firing
// for the child instance.

static TestTask test_master_reload_cascades_to_instances(TestContext& t) {
	eng->load_level("");

	const std::string master_path = "test_cascade_master.mm";
	const std::string child_path = "mats/test_cascade_child.mi";
	ScopedTempFile guard_master(master_path);
	ScopedTempFile guard_child(child_path);

	t.require(write_game_file(master_path,
							  "TYPE MaterialMaster\n"
							  "VAR vec4 colorMult 255 255 255 255\n"
							  "_FS_BEGIN\n"
							  "void FSmain() { BASE_COLOR = colorMult.rgb; NORMALMAP=vec3(0.5,0.5,1.0);"
							  " ROUGHNESS=0.8; METALLIC=0.0; AOMAP=1.0; }\n"
							  "_FS_END\n"),
			  "wrote master .mm");
	t.require(write_game_file(child_path,
							  "TYPE MaterialInstance\nPARENT " + master_path + "\nVAR colorMult 0 255 0 255\n"),
			  "wrote child .mi");

	auto master = g_assets.find<MaterialInstance>(master_path);
	auto child = g_assets.find<MaterialInstance>(child_path);
	t.require(master.is_valid(), "master .mm loaded");
	t.require(child.is_valid(), "child .mi loaded");

	int child_reload_count = 0;
	MaterialInstance* child_ptr = child.get();
	MaterialInstance::on_material_loaded.add(
		&child_reload_count, [&child_reload_count, child_ptr](MaterialInstance* m) {
			if (m == child_ptr)
				++child_reload_count;
		});

	g_assets.reload(master);
	co_await t.wait_ticks(1);

	MaterialInstance::on_material_loaded.remove(&child_reload_count);

	t.check(child_reload_count >= 1, "child .mi reloaded as part of master cascade");
	t.check(child.get() == child_ptr, "child MaterialInstance* unchanged across cascade");
}
GAME_TEST("assets/master_reload_cascades_to_instances", 20.f, test_master_reload_cascades_to_instances);

// ---------------------------------------------------------------------------
// Test 9: standalone-master (.mm without dependents) failed reload tombstones
// ---------------------------------------------------------------------------
// The full "master tombstones with live children" invariant from the plan
// (render as missing-material, no UB) cannot be exercised without first
// fixing a latent crash in the render path: `MaterialImpl::get_master_impl`
// dereferences `masterMaterial->impl` unconditionally, so once the master
// is tombstoned, any frame tick that hits a child crashes.  When that fix
// lands, expand this test to load a child .mi and assert no-crash after a
// frame tick.  For now, verify the asset-system half of the invariant:
// the corrupted master flips to did_load_fail() in place and stays
// addressable as the same MaterialInstance*.

static TestTask test_corrupted_master_reload_tombstones(TestContext& t) {
	const std::string master_path = "test_corrupt_master.mm";
	ScopedTempFile guard_master(master_path);

	t.require(write_game_file(master_path,
							  "TYPE MaterialMaster\n"
							  "VAR vec4 colorMult 255 255 255 255\n"
							  "_FS_BEGIN\n"
							  "void FSmain() { BASE_COLOR = colorMult.rgb; NORMALMAP=vec3(0.5,0.5,1.0);"
							  " ROUGHNESS=0.5; METALLIC=0.0; AOMAP=1.0; }\n"
							  "_FS_END\n"),
			  "wrote master .mm");

	auto master = g_assets.find<MaterialInstance>(master_path);
	t.require(master.is_valid(), "master loaded initially");
	MaterialInstance* master_ptr = master.get();

	// Corrupt the master file in a way that load_from_file will reject.
	t.require(write_game_file(master_path, "garbage not a material\n"), "corrupted master");

	g_assets.reload(master);

	t.check(master.get_unsafe()->did_load_fail(),
			"corrupted master is flagged failed after reload");
	t.check(!master.is_valid(), "AssetPtr<master>.is_valid() now false");
	t.check(master.get_unsafe() == master_ptr, "MaterialInstance* stable across failed reload");
	t.check(master.get_unsafe()->get_name() == master_path, "path preserved");
	co_return;
}
GAME_TEST("assets/corrupted_master_reload_tombstones", 10.f, test_corrupted_master_reload_tombstones);

// ---------------------------------------------------------------------------
// Test 10: Model reload walks the scene to refresh dependent components
// ---------------------------------------------------------------------------
// Model::post_load (reload branch) iterates every Component in the live
// Level and calls refresh_after_model_reload(this).  This is the hook
// animator components use to invalidate cached MSkeleton-derived state.
// Verifying the walk runs at all is exercised by reloading a model in a
// scene that contains a MeshComponent — if the walk crashes (bad cast,
// null deref, dangling level ptr) this test catches it.

static TestTask test_model_reload_walks_scene(TestContext& t) {
	eng->load_level("");
	auto* ent = eng->get_level()->spawn_entity();
	auto* mesh = ent->create_component<MeshComponent>();
	mesh->set_model_str("eng/cube.cmdl");

	co_await t.wait_ticks(1);
	t.require(mesh->get_model() != nullptr, "MeshComponent loaded eng/cube.cmdl");
	Model* m = const_cast<Model*>(mesh->get_model());

	// Reload triggers the scene walk inside Model::post_load on the reload branch.
	g_assets.reload<Model>(m);
	co_await t.wait_ticks(1);

	t.check(mesh->get_model() == m, "MeshComponent's Model* still valid after reload");
}
GAME_TEST("assets/model_reload_walks_scene", 15.f, test_model_reload_walks_scene);

// ---------------------------------------------------------------------------
// Test 11: Animator survives a hot-reload of its underlying skeletal model
// ---------------------------------------------------------------------------
// The agClipNode caches `seq` (a pointer into MSkeleton::clips) and `remap` (a
// pointer into MSkeleton::remaps) on first get_pose.  MSkeleton::uninstall
// wipes both vectors, so without scene-walk refresh those pointers would be
// dangling after reload and the next animator tick would segfault inside
// util_calc_rotations.
//
// This test: load a skeletal model, build a one-node animator with a real
// clip, tick once so the clip caches its remap, reload the model, tick again.
// The refresh chain (Model::post_load → MeshComponent::refresh_after_model_reload
// → AnimatorObject::refresh_after_model_reload → agClipNode::refresh_after_model_reload)
// invalidates the cached pointers; the post-reload tick re-runs set_clip via
// the test (because clipFrom matched the reloaded model, my refresh nulled seq
// — matching the documented contract that the caller re-issues set_clip).

static TestTask test_animator_survives_model_reload(TestContext& t) {
	eng->load_level("");

	auto* ent = eng->get_level()->spawn_entity();
	auto* mesh = ent->create_component<MeshComponent>();
	mesh->set_model_str("characters/swat_model/swat_model.cmdl"); // skeletal model used by top_down/player prefab
	co_await t.wait_ticks(1);

	Model* m = const_cast<Model*>(mesh->get_model());
	t.require(m != nullptr, "swat_model.cmdl loaded");
	t.require(m->get_skel() != nullptr, "model has a skeleton");

	// Grab any clip name from the skeleton.  If the asset truly has no clips
	// we'd skip the test rather than fail it, but in practice player_FINAL has
	// several baked clips.
	const auto& clips = m->get_skel()->get_all_clips();
	t.require(!clips.empty(), "skeleton has at least one clip baked in");
	const std::string clip_name = clips.begin()->first;

	// Minimal animator: one agClipNode as the root.
	agBuilder builder;
	agClipNode* clip = builder.alloc<agClipNode>();
	t.require(clip != nullptr, "agClipNode allocated");
	clip->set_clip(m, clip_name);
	builder.set_root(clip);

	AnimatorObject* anim = mesh->create_animator(&builder);
	t.require(anim != nullptr, "animator created");

	// Tick once so agClipNode::get_pose runs and populates has_init/remap.
	// Without this step we'd be exercising the no-cache path on reload, which
	// is uninteresting — the dangling pointers only exist after caching.
	co_await t.wait_ticks(1);

	const MSkeleton* skel_before = m->get_skel();

	// Reload triggers the scene walk + animator refresh chain.  agClipNode
	// invalidates remap=nullptr/has_init=false; since clipFrom matched, it
	// also nulls seq and clipFrom and logs an error.
	g_assets.reload<Model>(m);

	t.check(m->get_skel() == skel_before, "MSkeleton address stable across reload");
	t.check(mesh->get_animator() == anim, "AnimatorObject pointer stable across reload");

	// Per the plan's contract, the test re-issues set_clip after a clipFrom
	// reload.  Subsequent ticks must run without dereferencing the previously
	// dangling seq/remap.
	clip->set_clip(m, clip_name);
	co_await t.wait_ticks(1);

	t.check(mesh->get_animator() != nullptr, "animator survived a post-reload tick");
}
GAME_TEST("assets/animator_survives_model_reload", 20.f, test_animator_survives_model_reload);

// ---------------------------------------------------------------------------
// Test 12: AnimationSeqAsset::seq stays in sync across a Model reload
// ---------------------------------------------------------------------------
// AssetDatabase::reload() only reloads the single asset passed to it -- there's
// no dependency graph, so reloading a Model does NOT by itself cascade to any
// AnimationSeqAsset that resolved `seq` into that model's MSkeleton::clips.
// AnimationSeqAsset.cpp installs a global Model::on_model_loaded subscriber to
// close that gap. This test never touches the AnimationSeqAsset directly --
// only the cascade can be keeping it in sync.

static TestTask test_animseq_asset_survives_model_reload(TestContext& t) {
	eng->load_level("");

	auto* ent = eng->get_level()->spawn_entity();
	auto* mesh = ent->create_component<MeshComponent>();
	mesh->set_model_str("characters/swat_model/swat_model.cmdl");
	co_await t.wait_ticks(1);

	Model* m = const_cast<Model*>(mesh->get_model());
	t.require(m != nullptr, "swat_model.cmdl loaded");
	t.require(m->get_skel() != nullptr, "model has a skeleton");

	const auto& clips = m->get_skel()->get_all_clips();
	t.require(!clips.empty(), "skeleton has at least one clip baked in");
	const std::string clip_name = clips.begin()->first;
	const std::string asset_path = "characters/swat_model/swat_model/" + clip_name;

	auto seqAsset = g_assets.find<AnimationSeqAsset>(asset_path).get();
	t.require(seqAsset != nullptr, "AnimationSeqAsset resolved");
	t.require(seqAsset->seq != nullptr, "seq resolved before reload");

	g_assets.reload<Model>(m);
	co_await t.wait_ticks(1);

	t.check(seqAsset->seq != nullptr, "seq re-resolved after reload without touching the asset directly");
	t.check(seqAsset->srcModel.get() == m, "srcModel still points at the same (in-place reloaded) Model instance");
}
GAME_TEST("assets/animseq_asset_survives_model_reload", 20.f, test_animseq_asset_survives_model_reload);
