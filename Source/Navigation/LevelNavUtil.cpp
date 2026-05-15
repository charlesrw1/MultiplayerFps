// LevelNavUtil — sidecar load/save for the `.navmesh` blob, plus the editor bake entry point.
// Mirrors GameSceneGiUtil. The bake itself lives in NavMeshBaker.

#include "LevelNavUtil.h"
#include "RuntimeNavManager.h"

#include "recastnavigation/DetourNavMesh.h"
#include "recastnavigation/DetourStatus.h"

#include "Framework/Files.h"
#include "Framework/BinaryReadWrite.h"
#include "Framework/StringUtils.h"
#include "Debug.h"
#include "GameEnginePublic.h"
#include "Level.h"

#ifdef EDITOR_BUILD
#include "NavMeshBaker.h"
#endif

namespace {
constexpr int kNavSidecarMagic   = 0x4E4D5348; // 'NMSH'
constexpr int kNavSidecarVersion = 1;
const std::string kNavSidecarSuffix = ".navmesh";
}

bool LevelNavUtil::had_changes = false;

// Sidecar format (v1):
//   int32 magic    = 'NMSH'
//   int32 version  = 1
//   int32 tile_count
//   repeat tile_count:
//     int32 tile_byte_size
//     bytes tile_data  (dtCreateNavMeshData output)
//   v2 will append:
//     int32 layer_section_byte_size (0 in v1)
//     bytes layer_data
//
// We use dtNavMesh::addTile with DT_TILE_FREE_DATA so Detour owns the memory after handoff.

void LevelNavUtil::on_scene_load_nav(const std::string& mapname) {
	if (!RuntimeNavManager::inst) {
		sys_print(Warning, "LevelNavUtil::on_scene_load_nav: RuntimeNavManager not initialized\n");
		return;
	}
	std::string name = mapname;
	StringUtils::remove_extension(name);
	auto file = FileSys::open_read_game((name + kNavSidecarSuffix).c_str());
	if (!file) {
		sys_print(Debug, "scene has no baked navmesh\n");
		return;
	}
	BinaryReader reader(file.get());
	int magic   = reader.read_int32();
	int version = reader.read_int32();
	if (magic != kNavSidecarMagic) {
		sys_print(Warning, "navmesh sidecar: bad magic\n");
		return;
	}
	if (version != kNavSidecarVersion) {
		sys_print(Warning, "navmesh sidecar: version mismatch (file=%d expected=%d)\n", version, kNavSidecarVersion);
		return;
	}
	int tile_count = reader.read_int32();
	if (tile_count <= 0)
		return;

	DtNavMeshPtr mesh(dtAllocNavMesh());
	if (!mesh) {
		sys_print(Error, "navmesh sidecar: dtAllocNavMesh failed\n");
		return;
	}

	if (tile_count == 1) {
		// Single-tile path — baker writes the dtCreateNavMeshData blob directly, which already
		// carries its own header. dtNavMesh::init(data,size,DT_TILE_FREE_DATA) configures the
		// navmesh params from that header; the multi-tile init+addTile path would mismatch.
		int tile_size = reader.read_int32();
		if (tile_size <= 0) {
			sys_print(Warning, "navmesh sidecar: invalid tile size\n");
			return;
		}
		unsigned char* tile_bytes = (unsigned char*)dtAlloc(tile_size, DT_ALLOC_PERM);
		reader.read_bytes_ptr(tile_bytes, tile_size);
		if (dtStatusFailed(mesh->init(tile_bytes, tile_size, DT_TILE_FREE_DATA))) {
			sys_print(Error, "navmesh sidecar: dtNavMesh::init (single-tile) failed\n");
			dtFree(tile_bytes);
			return;
		}
	} else {
		dtNavMeshParams params{};
		params.orig[0] = 0; params.orig[1] = 0; params.orig[2] = 0;
		params.tileWidth  = 32.f;
		params.tileHeight = 32.f;
		params.maxTiles   = tile_count;
		params.maxPolys   = 1 << 16;
		if (dtStatusFailed(mesh->init(&params))) {
			sys_print(Error, "navmesh sidecar: dtNavMesh::init (tiled) failed\n");
			return;
		}
		for (int i = 0; i < tile_count; i++) {
			int tile_size = reader.read_int32();
			if (tile_size <= 0) {
				sys_print(Warning, "navmesh sidecar: invalid tile size\n");
				return;
			}
			unsigned char* tile_bytes = (unsigned char*)dtAlloc(tile_size, DT_ALLOC_PERM);
			reader.read_bytes_ptr(tile_bytes, tile_size);
			dtStatus s = mesh->addTile(tile_bytes, tile_size, DT_TILE_FREE_DATA, 0, nullptr);
			if (dtStatusFailed(s)) {
				sys_print(Warning, "navmesh sidecar: addTile failed for tile %d\n", i);
				dtFree(tile_bytes);
			}
		}
	}
	RuntimeNavManager::inst->set_navmesh_from_loading(std::move(mesh));
}

void LevelNavUtil::on_scene_exit() {
	if (RuntimeNavManager::inst)
		RuntimeNavManager::inst->clear();
}

void LevelNavUtil::bake_all_volumes() {
#ifdef EDITOR_BUILD
	NavMeshBaker baker;
	baker.bake_current_level();
	had_changes = true;
#else
	sys_print(Warning, "bake_all_volumes: editor-only\n");
#endif
}

void LevelNavUtil::save_to_disk() {
	if (!RuntimeNavManager::inst || !RuntimeNavManager::inst->has_navmesh()) {
		sys_print(Warning, "save_to_disk: no navmesh loaded\n");
		return;
	}
	const dtNavMesh* mesh = RuntimeNavManager::inst->get_navmesh();
	std::string name = eng->get_level()->get_source_asset_name();
	StringUtils::remove_extension(name);

	FileWriter writer;
	writer.write_int32(kNavSidecarMagic);
	writer.write_int32(kNavSidecarVersion);

	int tile_count = 0;
	for (int i = 0; i < mesh->getMaxTiles(); i++) {
		const dtMeshTile* t = mesh->getTile(i);
		if (t && t->header && t->dataSize > 0)
			tile_count++;
	}
	writer.write_int32(tile_count);
	for (int i = 0; i < mesh->getMaxTiles(); i++) {
		const dtMeshTile* t = mesh->getTile(i);
		if (!t || !t->header || t->dataSize <= 0)
			continue;
		writer.write_int32(t->dataSize);
		writer.write_bytes_ptr(t->data, t->dataSize);
	}

	IFilePtr out = FileSys::open_write_game(name + kNavSidecarSuffix);
	if (!out) {
		sys_print(Error, "save_to_disk: cannot open output\n");
		return;
	}
	out->write(writer.get_buffer(), writer.get_size());
}
