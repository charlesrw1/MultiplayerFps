#include "Prefab.h"
#include "Framework/Files.h"
#include "Framework/Util.h"

std::string PrefabFile::load_text(const std::string& game_relative_path) {
	auto file = FileSys::open_read_game(game_relative_path);
	if (!file) {
		sys_print(Warning, "Failed to open prefab file: %s\n", game_relative_path.c_str());
		return "";
	}

	size_t file_size = file->size();
	std::string text;
	text.resize(file_size);
	file->read(text.data(), file_size);
	file->close();

	return text;
}

bool PrefabFile::save_text(const std::string& game_relative_path, const std::string& text) {
	auto file = FileSys::open_write_game(game_relative_path);
	if (!file) {
		sys_print(Warning, "Failed to open prefab file for writing: %s\n", game_relative_path.c_str());
		return false;
	}

	if (!file->write(text.c_str(), text.size())) {
		sys_print(Warning, "Failed to write prefab file: %s\n", game_relative_path.c_str());
		file->close();
		return false;
	}

	file->close();
	return true;
}

#ifdef EDITOR_BUILD

// Register .tprefab in the asset browser
static auto _register_prefab_metadata = []() {
	auto metadata = new PrefabAssetMetadata();
	metadata->extensions.push_back("tprefab");
	AssetRegistrySystem::get().register_asset_type(metadata);
	return true;
}();

#endif  // EDITOR_BUILD
