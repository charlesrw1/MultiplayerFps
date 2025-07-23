#pragma once
#include "IAsset.h"
#include <memory>
#include <vector>
// A collection of assets that are loaded all at once
class AssetBundle : public IAsset {
public:
	CLASS_BODY(AssetBundle);
	bool load_asset(IAssetLoadingInterface* loading) final;
	void post_load() final {}
	void uninstall() final {}
	void move_construct(IAsset* other) final {
		if (auto as_bundle = other->cast_to<AssetBundle>()) {
			assets = std::move(as_bundle->assets);
		}
	}
private:
	std::vector<std::shared_ptr<IAsset>> assets;
};