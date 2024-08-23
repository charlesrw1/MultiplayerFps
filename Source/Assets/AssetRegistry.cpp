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

void AssetRegistrySystem::reindex_all_assets()
{
	all_disk_assets.clear();
	for (int i = 0; i < all_assettypes.size(); i++)
	{
		auto type = all_assettypes[i].get();
		std::vector<std::string> filepaths;
		type->index_assets(filepaths);
		bool is_filenames = type->assets_are_filepaths();

		std::string buf;
		for (int j = 0; j < filepaths.size(); j++) {
			AssetOnDisk aod;
			aod.filename = std::move(filepaths[j]);
			if (is_filenames) {
				buf.clear();
				buf = type->root_filepath() + aod.filename;
				auto file = FileSys::open_read_os(buf.c_str());
				if (!file) {
					sys_print("should be filename but isnt (this could be an uncompiled model, if so, ignore) %s\n", buf.c_str());
					//continue;
				}
				else
					aod.filesize = file->size();
			}
			aod.type = type;

			all_disk_assets.push_back(aod);
		}
	}

	root.reset();
	root = std::make_unique<AssetFilesystemNode>("root");

	for (auto a : all_disk_assets) {
		auto& filename = a.filename;
		std::vector<std::string> path = split(filename, '/');
		root->addPath(a,path);
	}
}
