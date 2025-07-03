#pragma once
#include "Framework/ClassBase.h"
#include "IAsset.h"

// too lazy, just hack the asset async loader to do this
struct AssetFilesystemNode;
class HackedAsyncAssetRegReindex : public IAsset {
public:
	CLASS_BODY(HackedAsyncAssetRegReindex);
	HackedAsyncAssetRegReindex();
	~HackedAsyncAssetRegReindex();
	void uninstall() override {

	}
	void move_construct(IAsset* other) override;
	void sweep_references(IAssetLoadingInterface*) const override {
	}
	bool load_asset(IAssetLoadingInterface*) override;
	void post_load() override;
	std::unique_ptr<AssetFilesystemNode> root;

	static bool is_in_loading;
	static std::unique_ptr<AssetFilesystemNode> root_to_clone;
};
