// Source/IntegrationTests/Tests/Editor/test_prefab_thumbnail.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "IntegrationTests/EditorTestContext.h"
#include "LevelEditor/EditorDocLocal.h"
#include "Framework/Files.h"
#include "Game/Prefab.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "LevelSerialization/SerializeNew.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetBrowser.h"
#include "Assets/AssetRegistry.h"
#include "Render/Texture.h"

// Prefab thumbnails must render every MeshComponent found anywhere in the prefab (not
// just the first one), each at its own world transform, and produce a loaded thumbnail
// texture. Regression guard for ThumbnailManager's PrefabAsset branch,
// PrefabAsset::find_all_components_by_type, and ThumbnailRenderer::render_multi.
static TestTask test_prefab_thumbnail_multi_mesh(TestContext& t) {
	const std::string prefab_path = "_test_thumb_prefab.tprefab";
	const char* prefab_path_cstr = prefab_path.c_str();
	FileSys::delete_game_file(prefab_path_cstr);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);

	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	const int baseline_entity_count = t.editor().entity_count();

	// Two entities, far apart, each with their own mesh. A thumbnail camera framed around
	// only the first mesh would badly crop this; the combined-bounds camera must frame both.
	EntityPtr e1 = editor->spawn_entity();
	e1->set_editor_name("ThumbA");
	e1->set_ls_position({-10.0f, 0.0f, 0.0f});
	e1->create_component<MeshComponent>()->set_model_str("eng/cube.cmdl");

	EntityPtr e2 = editor->spawn_entity();
	e2->set_editor_name("ThumbB");
	e2->set_ls_position({10.0f, 0.0f, 0.0f});
	e2->create_component<MeshComponent>()->set_model_str("eng/cube.cmdl");
	co_await t.wait_ticks(1);

	SerializedSceneFile ser;
	try {
		ser = NewSerialization::serialize_to_text("thumb_test_prefab", {e1.get(), e2.get()}, false,
												   prefab_path.c_str());
	}
	catch (const std::exception&) {
		t.require(false, "serialize prefab failed");
		co_return;
	}
	t.require(PrefabFile::save_text(prefab_path_cstr, ser.text), "prefab saved to disk");
	e1->destroy();
	e2->destroy();
	co_await t.wait_ticks(1);

	// Confirm the prefab's static tree really has two MeshComponents -- this is the data
	// find_all_components_by_type walks for thumbnail rendering.
	auto* prefab = PrefabAsset::load(prefab_path);
	t.require(prefab != nullptr, "prefab asset loads");
	auto mesh_comps = prefab->find_all_components_by_type(&MeshComponent::StaticType);
	t.require(mesh_comps.size() == 2, "prefab static tree has both mesh components");

	t.require(AssetBrowser::inst != nullptr, "asset browser exists");
	AssetOnDisk asset;
	asset.filename = prefab_path;
	asset.type = const_cast<AssetMetadata*>(AssetRegistrySystem::get().find_for_classtype(&PrefabAsset::StaticType));
	t.require(asset.type != nullptr, "prefab asset metadata registered");
	t.check(ThumbnailManager::supports_thumbnail(asset), "prefab reports thumbnail support");

	// First call just queues the entry. Drive the render+load steps directly so the test
	// doesn't depend on whether the Asset Browser window happens to be visible this frame.
	t.check(AssetBrowser::inst->thumbnails.get_thumbnail(asset) == nullptr, "not ready on first request");
	Texture* tex = nullptr;
	for (int i = 0; i < 10 && !tex; i++) {
		AssetBrowser::inst->thumbnails.tick();
		tex = AssetBrowser::inst->thumbnails.get_thumbnail(asset);
		co_await t.wait_ticks(1);
	}
	t.require(tex != nullptr, "prefab thumbnail texture loaded");
	t.check(tex->gpu_ptr != nullptr, "thumbnail texture uploaded to GPU");

	// Sanity check: thumbnail rendering must never touch the Level/Entity system (it renders
	// through a side path on draw.scene's shared render proxies -- see
	// ThumbnailRenderer::render_multi). This does NOT catch a render-proxy visibility leak
	// (those proxies aren't Entities), only that no entities were spawned/destroyed as a
	// side effect.
	t.check(t.editor().entity_count() == baseline_entity_count, "level entity count unaffected by thumbnail render");

	// Requesting the same thumbnail again (simulating repeated "Refresh Thumbnail" clicks)
	// must stay correct on the very next render, not require multiple refreshes to converge.
	AssetBrowser::inst->thumbnails.invalidate_thumbnail(prefab_path);
	AssetBrowser::inst->thumbnails.tick(); // Queued -> render + NeedLoad
	co_await t.wait_ticks(1);
	AssetBrowser::inst->thumbnails.tick(); // NeedLoad -> Loaded
	Texture* tex2 = AssetBrowser::inst->thumbnails.get_thumbnail(asset);
	t.check(tex2 != nullptr, "thumbnail re-renders to Loaded in a single refresh");

	FileSys::delete_game_file(prefab_path_cstr);
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/prefab_thumbnail_multi_mesh", 30.f, test_prefab_thumbnail_multi_mesh);
