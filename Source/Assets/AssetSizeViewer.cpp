#ifdef EDITOR_BUILD
#include "AssetSizeViewer.h"
#include "AssetRegistry.h"
#include "AssetRegistryLocal.h"
#include "Framework/Files.h"
#include "imgui.h"
#include <filesystem>
#include <algorithm>
#include <map>

namespace fs = std::filesystem;

AssetSizeViewer& AssetSizeViewer::get() {
	static AssetSizeViewer inst;
	return inst;
}

static const char* format_size(uint64_t bytes, char* buf, size_t buf_size) {
	if (bytes >= 1024ULL * 1024 * 1024)
		snprintf(buf, buf_size, "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
	else if (bytes >= 1024ULL * 1024)
		snprintf(buf, buf_size, "%.2f MB", bytes / (1024.0 * 1024.0));
	else if (bytes >= 1024ULL)
		snprintf(buf, buf_size, "%.2f KB", bytes / 1024.0);
	else
		snprintf(buf, buf_size, "%llu B", (unsigned long long)bytes);
	return buf;
}

static std::string get_folder(const std::string& path) {
	auto pos = path.rfind('/');
	if (pos == std::string::npos) return "";
	return path.substr(0, pos);
}

void AssetSizeViewer::refresh() {
	all_assets.clear();
	folder_groups.clear();
	total_size = 0;

	auto& reg = AssetRegistrySystem::get();
	const auto& linear = reg.get_linear_list();

	for (auto* node : linear) {
		if (node->is_folder()) continue;
		if (!node->asset.type) continue;

		auto full = FileSys::get_full_path_from_game_path(node->asset.filename);
		std::error_code ec;
		auto sz = fs::file_size(full, ec);
		if (ec) continue;

		SizedAsset sa;
		sa.path = node->asset.filename;
		sa.folder = get_folder(sa.path);
		sa.size_bytes = sz;
		sa.type_index = (int)node->asset.type->self_index;
		total_size += sz;
		all_assets.push_back(std::move(sa));
	}

	std::sort(all_assets.begin(), all_assets.end(),
		[](const SizedAsset& a, const SizedAsset& b) { return a.size_bytes > b.size_bytes; });

	std::map<std::string, FolderGroup> grouped;
	for (auto& a : all_assets) {
		auto& g = grouped[a.folder];
		g.name = a.folder;
		g.total_size += a.size_bytes;
		g.assets.push_back(&a);
	}

	folder_groups.clear();
	for (auto& [k, v] : grouped)
		folder_groups.push_back(std::move(v));

	std::sort(folder_groups.begin(), folder_groups.end(),
		[](const FolderGroup& a, const FolderGroup& b) { return a.total_size > b.total_size; });

	needs_refresh = false;
}

void AssetSizeViewer::imgui_draw() {
	if (!is_open) return;
	if (needs_refresh) refresh();

	ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Asset Size Viewer", &is_open)) {
		ImGui::End();
		return;
	}

	char buf[64];
	ImGui::Text("Total: %s  (%zu assets)", format_size(total_size, buf, sizeof(buf)), all_assets.size());
	ImGui::SameLine();
	if (ImGui::Button("Refresh")) needs_refresh = true;
	ImGui::Separator();

	draw_treemap();

	ImGui::End();
}

static ImU32 get_type_color(int type_index) {
	auto& types = AssetRegistrySystem::get().get_types();
	if (type_index >= 0 && type_index < (int)types.size()) {
		auto c = types[type_index]->get_browser_color();
		return IM_COL32(c.r, c.g, c.b, 200);
	}
	return IM_COL32(128, 128, 128, 200);
}

static ImU32 get_type_color_hover(int type_index) {
	auto& types = AssetRegistrySystem::get().get_types();
	if (type_index >= 0 && type_index < (int)types.size()) {
		auto c = types[type_index]->get_browser_color();
		int r = std::min(255, c.r + 40);
		int g = std::min(255, c.g + 40);
		int b = std::min(255, c.b + 40);
		return IM_COL32(r, g, b, 230);
	}
	return IM_COL32(168, 168, 168, 230);
}

struct TreemapRect { float x, y, w, h; };

struct TreemapItem {
	float value;
	int original_index;
};

static void layout_squarify(const std::vector<TreemapItem>& items, TreemapRect area,
	std::vector<TreemapRect>& out_rects) {
	out_rects.resize(items.size());
	if (items.empty()) return;

	float total = 0;
	for (auto& it : items) total += it.value;
	if (total <= 0) return;

	float rect_x = area.x, rect_y = area.y, rect_w = area.w, rect_h = area.h;
	size_t start = 0;

	while (start < items.size()) {
		bool vertical = rect_w >= rect_h;
		float side = vertical ? rect_h : rect_w;
		if (side <= 0) break;

		float remaining = 0;
		for (size_t i = start; i < items.size(); i++) remaining += items[i].value;
		if (remaining <= 0) break;
		float scale = (rect_w * rect_h) / remaining;

		float row_sum = 0;
		float best_aspect = 1e30f;
		size_t row_end = start;

		for (size_t i = start; i < items.size(); i++) {
			float val = items[i].value * scale;
			float new_sum = row_sum + val;
			float row_side = new_sum / side;

			float worst = 0;
			float temp_sum = 0;
			for (size_t j = start; j <= i; j++) {
				float v = items[j].value * scale;
				float other_side = v / row_side;
				float aspect = (row_side > other_side) ? row_side / other_side : other_side / row_side;
				if (aspect > worst) worst = aspect;
			}

			if (worst <= best_aspect) {
				best_aspect = worst;
				row_sum = new_sum;
				row_end = i + 1;
			} else {
				break;
			}
		}

		if (row_end == start) row_end = start + 1;

		row_sum = 0;
		for (size_t i = start; i < row_end; i++) row_sum += items[i].value * scale;
		float row_thick = row_sum / side;

		float pos = 0;
		for (size_t i = start; i < row_end; i++) {
			float val = items[i].value * scale;
			float len = (row_thick > 0) ? val / row_thick : 0;
			int idx = items[i].original_index;
			if (vertical) {
				out_rects[idx] = { rect_x, rect_y + pos, row_thick, len };
			} else {
				out_rects[idx] = { rect_x + pos, rect_y, len, row_thick };
			}
			pos += len;
		}

		if (vertical) {
			rect_x += row_thick;
			rect_w -= row_thick;
		} else {
			rect_y += row_thick;
			rect_h -= row_thick;
		}

		start = row_end;
	}
}

void AssetSizeViewer::draw_treemap() {
	if (folder_groups.empty()) return;

	auto avail = ImGui::GetContentRegionAvail();
	float map_h = avail.y;
	float map_w = avail.x;
	if (map_w < 100 || map_h < 100) return;

	auto cursor = ImGui::GetCursorScreenPos();
	auto* dl = ImGui::GetWindowDrawList();

	TreemapRect full_area = { cursor.x, cursor.y, map_w, map_h };

	std::vector<TreemapItem> folder_items;
	for (int i = 0; i < (int)folder_groups.size(); i++)
		folder_items.push_back({ (float)folder_groups[i].total_size, i });

	std::vector<TreemapRect> folder_rects;
	layout_squarify(folder_items, full_area, folder_rects);

	ImGui::InvisibleButton("##treemap", ImVec2(map_w, map_h));
	auto mouse = ImGui::GetMousePos();
	char buf[64];

	for (int fi = 0; fi < (int)folder_groups.size(); fi++) {
		auto& fg = folder_groups[fi];
		auto& fr = folder_rects[fi];
		if (fr.w < 1 || fr.h < 1) continue;

		const float border = 1.0f;
		TreemapRect inner = { fr.x + border, fr.y + border, fr.w - border * 2, fr.h - border * 2 };
		if (inner.w < 1 || inner.h < 1) continue;

		dl->AddRect(ImVec2(fr.x, fr.y), ImVec2(fr.x + fr.w, fr.y + fr.h),
			IM_COL32(255, 255, 255, 180), 0, 0, 1.5f);

		std::vector<TreemapItem> asset_items;
		for (int ai = 0; ai < (int)fg.assets.size(); ai++)
			asset_items.push_back({ (float)fg.assets[ai]->size_bytes, ai });

		std::vector<TreemapRect> asset_rects;
		layout_squarify(asset_items, inner, asset_rects);

		for (int ai = 0; ai < (int)fg.assets.size(); ai++) {
			auto& ar = asset_rects[ai];
			auto* asset = fg.assets[ai];
			if (ar.w < 1 || ar.h < 1) continue;

			bool hovered = mouse.x >= ar.x && mouse.x < ar.x + ar.w &&
				mouse.y >= ar.y && mouse.y < ar.y + ar.h;

			ImU32 col = hovered ? get_type_color_hover(asset->type_index) : get_type_color(asset->type_index);
			dl->AddRectFilled(ImVec2(ar.x + 0.5f, ar.y + 0.5f),
				ImVec2(ar.x + ar.w - 0.5f, ar.y + ar.h - 0.5f), col);

			if (ar.w > 30 && ar.h > 14) {
				auto name_pos = asset->path.rfind('/');
				const char* name = (name_pos != std::string::npos)
					? asset->path.c_str() + name_pos + 1 : asset->path.c_str();

				auto text_size = ImGui::CalcTextSize(name);
				if (text_size.x < ar.w - 4 && text_size.y < ar.h - 2) {
					dl->AddText(ImVec2(ar.x + 2, ar.y + 1), IM_COL32(255, 255, 255, 255), name);
				}
			}

			if (hovered) {
				ImGui::BeginTooltip();
				ImGui::Text("%s", asset->path.c_str());
				ImGui::Text("%s", format_size(asset->size_bytes, buf, sizeof(buf)));
				if (asset->type_index >= 0) {
					auto& types = AssetRegistrySystem::get().get_types();
					if (asset->type_index < (int)types.size())
						ImGui::Text("Type: %s", types[asset->type_index]->get_type_name().c_str());
				}
				ImGui::EndTooltip();
			}
		}

		if (inner.w > 40 && inner.h > 24) {
			const char* folder_label = fg.name.empty() ? "(root)" : fg.name.c_str();
			std::string label = std::string(folder_label) + " [" + format_size(fg.total_size, buf, sizeof(buf)) + "]";
			auto text_size = ImGui::CalcTextSize(label.c_str());

			float tx = inner.x + (inner.w - text_size.x) * 0.5f;
			float ty = inner.y + (inner.h - text_size.y) * 0.5f;
			dl->AddRectFilled(ImVec2(tx - 2, ty - 1), ImVec2(tx + text_size.x + 2, ty + text_size.y + 1),
				IM_COL32(0, 0, 0, 160));
			dl->AddText(ImVec2(tx, ty), IM_COL32(255, 255, 255, 255), label.c_str());
		}
	}
}

#endif
