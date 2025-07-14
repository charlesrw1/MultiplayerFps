#include "AssetBundle.h"
#include "AssetDatabase.h"
#include "Framework/Files.h"
#include "Framework/StringUtils.h"
#include "Framework/DictParser.h"
#include "IAsset.h"
bool AssetBundle::load_asset(IAssetLoadingInterface* loading)
{
	auto file = FileSys::open_read_game(get_name());
	if (!file) {
		sys_print(Error, "AssetBundle::load_asset: no path %s\n", get_name().c_str());
		return false;
	}
	string fileStr(file->size(), ' ');
	file->read(fileStr.data(), fileStr.size());
	auto lines = StringUtils::to_lines(fileStr);
	for (auto& line : lines) {
		auto toks = StringUtils::split(line);
		if (toks.size() == 2) {
			auto class_type = ClassBase::find_class(toks.at(0).c_str());
			if (class_type&&class_type->is_a(IAsset::StaticType)) {
				assets.push_back(loading->load_asset(class_type, toks.at(1)));
			}
			else {
				sys_print(Error, "AssetBundle::load_asset: no IAsset type %s\n", toks.at(0).c_str());
			}
		}
	}
	return true;
}

