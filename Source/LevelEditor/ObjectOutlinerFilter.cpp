// Filter/search logic for the Object Outliner (OONameFilter implementation).
#ifdef EDITOR_BUILD
#include "EditorDocLocal.h"
#include "Framework/MyImguiLib.h"
#include "ObjectOutlineFilter.h"
#include "Game/Components/SpawnerComponenth.h"
#include <sstream>

static void HelpMarker(const char* desc) {
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip()) {
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void OONameFilter::draw() {
	ASSERT(true); // precondition: ImGui frame is active
	if (std_string_input_text("##filter", filter_component, ImGuiInputTextFlags_EnterReturnsTrue)) {
		filter_component = filter_component.c_str();
		on_filter_enter.invoke(filter_component);
	}
	ImGui::SameLine();
	HelpMarker("Filter scene.\n"
			   "Searches for:\n"
			   "\tentity name\n"
			   "\tentity id\n"
			   "\tcomponent name\n"
			   "\tasset name\n"
			   "\tprefab name\n"
			   "\tentity ptr is\n"
			   "Searches for all categories automatically.\n"
			   "Supprts AND and OR searches:\n"
			   "\t\"mymesh | physics somename\" searches for mymesh OR (physics AND somename)\n"
			   "Not case sensitive.\n"
			   "Return to confirm filter.\n");
}

// dumb but want something that just works
bool OONameFilter::is_in_string(const string& filter, const string& match) {
	ASSERT(!filter.empty() || filter.empty()); // always valid — no precondition
	auto lowercased = to_lower(match);
	return lowercased.find(filter) != std::string::npos;
}

static bool check_props_for_assetptr_or_entityptr(const string& filter, void* inst, const PropertyInfoList* list) {
	ASSERT(inst != nullptr);
	ASSERT(list != nullptr);
	for (int i = 0; i < list->count; i++) {
		auto& prop = list->list[i];
		if (prop.type == core_type_id::AssetPtr) {
			IAsset** asset = (IAsset**)prop.get_ptr(inst);
			if (*asset) {
				if (OONameFilter::is_in_string(filter, (*asset)->get_name())) // asset name
					return true;
			}
		} else if (prop.type == core_type_id::ObjHandlePtr) {
			obj<BaseUpdater>* eptr = (obj<BaseUpdater>*)prop.get_ptr(inst);
			BaseUpdater* what = eptr->get();
			if (what &&
				OONameFilter::is_in_string(filter, std::to_string(what->get_instance_id()))) // entity ptr instance id
				return true;
		} else if (prop.type == core_type_id::List) {
			auto listptr = prop.get_ptr(inst);
			auto size = prop.list_ptr->get_size(listptr);
			for (int j = 0; j < size; j++) {
				auto ptr = prop.list_ptr->get_index(listptr, j);
				bool b = check_props_for_assetptr_or_entityptr(filter, ptr, prop.list_ptr->props_in_list);
				if (b)
					return true;
			}
		}
	}
	return false;
}

bool OONameFilter::does_entity_pass_one_filter(const string& filter, Entity* e) {
	ASSERT(e != nullptr);
	// check: name of object, component names, id of object
	// props: asset names, ptr names/ids
	if (is_in_string(filter, e->get_editor_name())) // name of object
		return true;
	for (auto& c : e->get_components()) {
		if (is_in_string(filter, c->get_type().classname)) // names of components
			return true;
	}

	if (is_in_string(filter, std::to_string(e->get_instance_id()))) // instance id
		return true;

	// now check properties
	for (auto& c : e->get_components()) {
		if (auto sc = c->cast_to<SpawnerComponent>()) {
			if (is_in_string(filter, sc->get_spawner_type()))
				return true;
		}

		auto type = &c->get_type();
		while (type) {
			const PropertyInfoList* p = type->props;
			if (p) {
				bool res = check_props_for_assetptr_or_entityptr(filter, c, p);
				if (res)
					return true;
			}
			type = type->super_typeinfo;
		}
	}

	return false; // no matches
}

bool OONameFilter::does_entity_pass(const vector<vector<string>>& filter, Entity* e) {
	ASSERT(e != nullptr);
	if (filter.empty())
		return true;
	for (auto& ors : filter) {
		bool res = true;
		for (auto& ands : ors) {
			if (!does_entity_pass_one_filter(ands, e)) {
				res = false;
				break;
			}
		}
		if (res)
			return true;
	}
	return false;
}

vector<vector<string>> OONameFilter::parse_into_and_ors(const std::string& filter) {
	ASSERT(true); // no preconditions; empty string yields empty result
	std::vector<std::vector<std::string>> result;
	std::stringstream ss(filter);
	std::string segment;

	while (std::getline(ss, segment, '|')) { // Split by '|'
		std::vector<std::string> subVector;
		std::stringstream subSS(segment);
		std::string word;

		while (subSS >> word) { // Split by spaces within segments
			subVector.push_back(to_lower(word));
		}

		result.push_back(subVector);
	}

	return result;
}
#endif
