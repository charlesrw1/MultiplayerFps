#pragma once
#include "IAsset.h"
// A collection of assets that are loaded all at once
class AssetBundle : public IAsset {
public:
	CLASS_BODY(AssetBundle);
	bool load_asset(IAssetLoadingInterface* loading) final;
	void sweep_references(IAssetLoadingInterface* loading) const final;
	void post_load() final {}
	void uninstall() final {}
	void move_construct(IAsset* other) final {
		if (auto as_bundle = other->cast_to<AssetBundle>()) {
			assets = std::move(as_bundle->assets);
		}
	}
private:
	std::vector<const IAsset*> assets;
};