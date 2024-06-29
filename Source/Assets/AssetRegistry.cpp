#include "AssetRegistry.h"
#include "Framework/Files.h"
AssetRegistrySystem& AssetRegistrySystem::get()
{
	static AssetRegistrySystem inst;
	return inst;
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
					sys_print("should be filename but isnt %s\n", buf.c_str());
					continue;
				}
				aod.filesize = file->size();
			}
			aod.type = type;

			all_disk_assets.push_back(aod);
		}

	}
}
