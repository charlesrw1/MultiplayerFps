#pragma once
#include "Game/EntityComponent.h"
#include "Framework/StructReflection.h"
#include "Animation/Editor/Optional.h"
#include "Framework/MapUtil.h"
struct EditorFolder {
	STRUCT_BODY();
	REF std::string folderName;
	REF int8_t id = 0;
	REF bool is_hidden = false;
	REF bool is_folder_open = true;	// save the state here? i guess
};

// this is a data container component for the editor
// lets editor save stuff with the map
class EditorMapDataComponent : public Component {
public:
	CLASS_BODY(EditorMapDataComponent);

	static const int MAX_FOLDERS = 100;

	EditorFolder* lookup_for_id(int8_t id) {
		auto index = find_folder_index(id);
		if (index.has_value())
			return &folders.at(index.value());
		return nullptr;
	}
	void remove_folder(int8_t id) {
		auto index = find_folder_index(id);
		if (index.has_value()) {
			folders.erase(folders.begin() + index.value());
		}
		else {
			sys_print(Warning, "EditorMapDataComponent::remove_folder(%d): not found\n", int(id));
		}
	}
	void clear_invalid_folders() {
		for (int i = 0; i < (int)folders.size(); i++) {
			if (folders[i].id == 0) {
				folders.erase(folders.begin() + i);
				i--;
			}
		}
	}
	EditorFolder* add_folder() {
		// kinda dumb
		std::unordered_set<int8_t> existing;
		int8_t max = 1;
		for (auto& f : folders) {
			if (SetUtil::contains(existing, f.id)) {
				sys_print(Warning, "EditorMapDataComponent: duplicate folder ids %d\n", int(f.id));
				f.id = 0;	// fix after
			}
			else {
				SetUtil::insert_test_exists(existing, f.id);
				max = std::max(max, f.id);
			}
		}
		clear_invalid_folders();

		for (int i = 0; i < MAX_FOLDERS; i++) {
			const int idToCheck = (int(max) + i) % MAX_FOLDERS;
			if (idToCheck == 0) 
				continue;
			if (!SetUtil::contains(existing, int8_t(idToCheck))) {
				EditorFolder newF;
				newF.id = idToCheck;
				folders.push_back(newF);
				return &folders.back();
			}
		}
		sys_print(Warning, "EditorMapDataComponent: couldnt make folder\n");
		return nullptr;
	}
	opt<int> find_folder_index(int8_t id) {
		for (int i = 0; i < folders.size(); i++) {
			if (folders[i].id == id)
				return i;
		}
		return std::nullopt;
	}
	const std::vector<EditorFolder>& get_folders() const {
		return folders;
	}
private:
	REFLECT(hide);
	std::vector<EditorFolder> folders;
};