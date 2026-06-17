#pragma once
#ifdef EDITOR_BUILD

class AssetSizeViewer
{
public:
	static AssetSizeViewer& get();

	void imgui_draw();
	void open() { is_open = true; needs_refresh = true; }

	bool is_open = false;

private:
	struct SizedAsset
	{
		std::string path;
		std::string folder;
		uint64_t size_bytes = 0;
		int type_index = -1;
	};

	struct FolderGroup
	{
		std::string name;
		uint64_t total_size = 0;
		std::vector<const SizedAsset*> assets;
	};

	void refresh();
	void draw_treemap();

	std::vector<SizedAsset> all_assets;
	std::vector<FolderGroup> folder_groups;
	uint64_t total_size = 0;
	bool needs_refresh = true;
};

#endif
