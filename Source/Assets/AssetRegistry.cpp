#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"
#include <algorithm>
#include <unordered_map>
#include <sstream>
AssetRegistrySystem& AssetRegistrySystem::get()
{
	static AssetRegistrySystem inst;
	return inst;
}




// Helper function to split a string by a delimiter
std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}
#include "AssetCompile/Someutils.h"
#include <unordered_set>
#include <cassert>
void AssetRegistrySystem::reindex_all_assets()
{
	std::vector<AssetOnDisk> diskAssets;
	diskAssets.clear();
	const int len = strlen(FileSys::get_game_path());
	for (int i = 0; i < all_assettypes.size(); i++)
	{
		auto type = all_assettypes[i].get();

		bool is_filenames = type->assets_are_filepaths();

		std::unordered_set<std::string> added_already;

		if (is_filenames) {
			for (const auto& file : FileSys::find_game_files()) {
				for (int j = 0; j < type->extensions.size(); j++) {
					if (type->extensions[j] == get_extension_no_dot(file)) {
						if (added_already.find(file) != added_already.end())
							continue;

						AssetOnDisk aod;
						aod.filename = file;
						if (is_filenames) {
							if (aod.filename.find(FileSys::get_game_path()) == 0)
								aod.filename = aod.filename.substr(len + 1);
						}
						aod.type = type;

						diskAssets.push_back(aod);

						added_already.insert(file);
					}
				}
				if (!type->pre_compilied_extension.empty()) {
					assert(type->extensions.size() != 0);
					if (type->pre_compilied_extension == get_extension_no_dot(file)) {
						auto path = strip_extension(file) + "." +type->extensions.at(0);	// unsafe
						if (added_already.find(path) == added_already.end()) {
							AssetOnDisk aod;
							aod.filename = path;
							if (is_filenames) {
								if (aod.filename.find(FileSys::get_game_path()) == 0)
									aod.filename = aod.filename.substr(len + 1);
							}
							aod.type = type;

							diskAssets.push_back(aod);

							added_already.insert(path);
						}
					}
				}
			}
		}
		std::vector<std::string> extraAssets;
		type->fill_extra_assets(extraAssets);
		for (int j = 0; j < extraAssets.size(); j++) {
			AssetOnDisk aod;
			aod.filename = std::move(extraAssets[j]);
			aod.type = type;
			diskAssets.push_back(aod);
		}
	}

	root.reset();
	root = std::make_unique<AssetFilesystemNode>("root");

	for (auto a : diskAssets) {
		auto& filename = a.filename;
		std::vector<std::string> path = split(filename, '/');
		root->addPath(a,path);
	}
}


const ClassTypeInfo* AssetRegistrySystem::find_asset_type_for_ext(const std::string& ext)
{
	for (auto& type : all_assettypes) {
		for (auto& ext_ : type->extensions)
			if (ext_ == ext)
				return type->get_asset_class_type();
	}
	return nullptr;
}
#include "Framework/Config.h"
#include "Assets/AssetDatabase.h"
DECLARE_ENGINE_CMD(TOUCH_ASSET)
{
	if (args.size() != 2) {
		sys_print(Warning, "TOUCH_ASSET <asset path>\n");
		return;
	}
	auto type = AssetRegistrySystem::get().find_asset_type_for_ext(get_extension_no_dot(args.at(1)));
	if (type) {
		auto res = GetAssets().find_sync(args.at(1), type, 0);
		if (!res)
			sys_print(Error, "TOUCH_ASSET failed\n");
	}
	else {
		sys_print(Error, "couldnt find type\n");
	}
}
#endif