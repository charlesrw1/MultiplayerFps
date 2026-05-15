#ifdef EDITOR_BUILD
#include "EditorRecents.h"
#include "Framework/Config.h"
#include "Framework/Files.h"
#include "Framework/Util.h"
#include <json.hpp>

EditorRecents g_editor_recents;

static const char* RECENTS_FILE = "editor_recents.json";

static nlohmann::json vec3_to_json(const glm::vec3& v) {
	return nlohmann::json::array({v.x, v.y, v.z});
}

static glm::vec3 vec3_from_json(const nlohmann::json& j, const glm::vec3& fallback) {
	if (!j.is_array() || j.size() != 3) return fallback;
	return glm::vec3{j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

static nlohmann::json snapshot_to_json(const CameraSnapshot& s) {
	nlohmann::json j;
	j["is_ortho"] = s.is_ortho;
	j["position"] = vec3_to_json(s.position);
	j["front"] = vec3_to_json(s.front);
	j["up"] = vec3_to_json(s.up);
	j["yaw"] = s.yaw;
	j["pitch"] = s.pitch;
	j["orbit_target"] = vec3_to_json(s.orbit_target);
	j["distance"] = s.distance;
	j["orbit_mode"] = s.orbit_mode;
	j["ortho_position"] = vec3_to_json(s.ortho_position);
	j["ortho_front"] = vec3_to_json(s.ortho_front);
	j["ortho_up"] = vec3_to_json(s.ortho_up);
	j["ortho_side"] = vec3_to_json(s.ortho_side);
	j["ortho_width"] = s.ortho_width;
	return j;
}

static CameraSnapshot snapshot_from_json(const nlohmann::json& j) {
	CameraSnapshot s;
	s.is_ortho = j.value("is_ortho", false);
	s.position = vec3_from_json(j.value("position", nlohmann::json{}), s.position);
	s.front = vec3_from_json(j.value("front", nlohmann::json{}), s.front);
	s.up = vec3_from_json(j.value("up", nlohmann::json{}), s.up);
	s.yaw = j.value("yaw", 0.0f);
	s.pitch = j.value("pitch", 0.0f);
	s.orbit_target = vec3_from_json(j.value("orbit_target", nlohmann::json{}), s.orbit_target);
	s.distance = j.value("distance", 0.0f);
	s.orbit_mode = j.value("orbit_mode", false);
	s.ortho_position = vec3_from_json(j.value("ortho_position", nlohmann::json{}), s.ortho_position);
	s.ortho_front = vec3_from_json(j.value("ortho_front", nlohmann::json{}), s.ortho_front);
	s.ortho_up = vec3_from_json(j.value("ortho_up", nlohmann::json{}), s.ortho_up);
	s.ortho_side = vec3_from_json(j.value("ortho_side", nlohmann::json{}), s.ortho_side);
	s.ortho_width = j.value("ortho_width", 25.0f);
	return s;
}

void EditorRecents::record(std::string path, const CameraSnapshot& cam) {
	if (path.empty()) return;
	for (auto it = entries.begin(); it != entries.end(); ++it) {
		if (it->path == path) {
			entries.erase(it);
			break;
		}
	}
	entries.push_front(RecentDocEntry{std::move(path), cam});
	while ((int)entries.size() > MAX_ENTRIES)
		entries.pop_back();
	save();
}

std::optional<RecentDocEntry> EditorRecents::at_slot(int one_indexed_slot) const {
	if (one_indexed_slot < 1 || one_indexed_slot > (int)entries.size())
		return std::nullopt;
	return entries[one_indexed_slot - 1];
}

void EditorRecents::print_list(const Cmd_Args& args) const {
	if (entries.empty()) {
		args.sys_print(Info, "No recent documents.\n");
		return;
	}
	args.sys_print(Info, "Recent documents:\n");
	int slot = 1;
	for (const auto& e : entries) {
		args.sys_print(Info, "  %2d  %s\n", slot, e.path.c_str());
		++slot;
	}
}

void EditorRecents::load() {
	entries.clear();
	auto file = FileSys::open_read_game(RECENTS_FILE);
	if (!file)
		return; // missing file is fine; no recents yet
	std::string text(file->size(), ' ');
	file->read(text.data(), file->size());
	file->close();
	try {
		auto j = nlohmann::json::parse(text);
		if (!j.is_array()) {
			sys_print(Warning, "EditorRecents::load: %s root is not an array\n", RECENTS_FILE);
			return;
		}
		for (const auto& item : j) {
			if (!item.is_object()) continue;
			RecentDocEntry e;
			e.path = item.value("path", std::string{});
			if (e.path.empty()) continue;
			if (item.contains("camera"))
				e.camera = snapshot_from_json(item["camera"]);
			entries.push_back(std::move(e));
			if ((int)entries.size() >= MAX_ENTRIES) break;
		}
	} catch (const std::exception& ex) {
		sys_print(Warning, "EditorRecents::load: parse error: %s\n", ex.what());
		entries.clear();
	}
}

void EditorRecents::save() const {
	nlohmann::json j = nlohmann::json::array();
	for (const auto& e : entries) {
		nlohmann::json item;
		item["path"] = e.path;
		item["camera"] = snapshot_to_json(e.camera);
		j.push_back(std::move(item));
	}
	std::string text = j.dump(2);
	auto file = FileSys::open_write_game(RECENTS_FILE);
	if (!file) {
		sys_print(Warning, "EditorRecents::save: failed to open %s for writing\n", RECENTS_FILE);
		return;
	}
	file->write(text.c_str(), text.size());
	file->close();
}
#endif
