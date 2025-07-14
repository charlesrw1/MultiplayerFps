#pragma once
#include "Framework/ClassBase.h"
#include "IAsset.h"

struct AssetFilesystemNode;
class HackedAsyncAssetRegReindex  {
public:
	bool load_asset(IAssetLoadingInterface*, AssetFilesystemNode& rootToClone);
	void post_load();
	std::unique_ptr<AssetFilesystemNode> root;

};
