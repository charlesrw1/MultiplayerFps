#include "EditorDocLocal.h"
#include "Commands.h"
#include "Assets/AssetDatabase.h"
#include "EditorPopups.h"
#include "Framework/Files.h"
#include "Framework/MyImguiLib.h"
#include "ObjectOutlineFilter.h"

ObjectOutliner::~ObjectOutliner() {

}

void ObjectOutliner::rebuild_tree() {
	delete_tree();

	auto the_filter = OONameFilter::parse_into_and_ors(filter->get_filter());

	rootnode = std::make_unique<Node>();
	auto level = eng->get_level();
	auto& all_objs = level->get_all_objects();
	for (auto ent : all_objs) {
		if (Entity* e = ent->cast_to<Entity>()) {
			if (!e->get_parent() && !e->dont_serialize_or_edit) {
				rootnode->add_child(std::make_unique<Node>(this, e, the_filter));
			
				if (!rootnode->children.back()->is_visible) {
					rootnode->children.pop_back();
					num_nodes--;
				}
			}
		}
	}
	rootnode->sort_children();

}
ObjectOutliner::Node::Node(ObjectOutliner* oo, Entity* initfrom, const std::vector<std::vector<std::string>>& filter) {
	did_pass_filter = OONameFilter::does_entity_pass(filter, initfrom);

	ptr = initfrom->get_self_ptr();
	auto& children = initfrom->get_children();
	bool do_any_children_pass = false;
	if (oo->should_draw_children(initfrom)) {
		for (auto& c : children) {
			if (!c->dont_serialize_or_edit) {
				add_child(std::make_unique<Node>(oo, c, filter));
				do_any_children_pass = do_any_children_pass || this->children.back()->did_pass_filter;
				if (!this->children.back()->is_visible) {
					this->children.pop_back();	// uptr does destruction
					oo->num_nodes--;
				}
			}
		}
		//if(!initfrom->get_parent())
		//	sort_children();
	}

	is_visible = did_pass_filter || do_any_children_pass;

	oo->num_nodes++;
}


ObjectOutliner::ObjectOutliner(EditorDoc& ed_doc) : ed_doc(ed_doc)
{
	ed_doc.on_close.add(this, &ObjectOutliner::on_close);
	ed_doc.on_start.add(this, &ObjectOutliner::on_start);
	ed_doc.post_node_changes.add(this, &ObjectOutliner::on_changed_ents);
	ed_doc.selection_state->on_selection_changed.add(this, &ObjectOutliner::on_selection_change);

	filter = std::make_unique<OONameFilter>();
	filter->on_filter_enter.add(this, [this](std::string s)
		{
			rebuild_tree();
		});
}

void ObjectOutliner::on_selection_change()
{
	if (ed_doc.selection_state->has_only_one_selected()) {
		setScrollHere = ed_doc.selection_state->get_only_one_selected();
	}
}

void ObjectOutliner::init()
{
	hidden = g_assets.find_global_sync<Texture>("eng/editor/hidden.png");
	visible = g_assets.find_global_sync<Texture>("eng/editor/visible.png");
}

bool ObjectOutliner::IteratorDraw::step()
{
	if (child_index >= node->children.size() && !node->parent)
		return false;
	else if (child_index >= node->children.size())
	{
		node = node->parent;
		child_index = child_stack.back();
		child_stack.pop_back();
		return step();
	}
	else {
		int i = child_index++;
		child_stack.push_back(child_index);
		child_index = 0;
		node = node->children.at(i).get();
	}
	return true;
}

static void save_off_branch_as_scene(EditorDoc& ed_doc, Entity* e);
void ObjectOutliner::IteratorDraw::draw(EditorDoc& ed_doc)
{

	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	int depth = child_stack.size();
	ImGui::Dummy(ImVec2(depth * 10.f, 0));
	ImGui::SameLine();
	auto n = node;
	Entity* node_entity = n->ptr.get();
	const bool is_root_node = node_entity == nullptr;
	ImGui::PushID(n);
	{
		const bool item_is_selected = ed_doc.selection_state->is_entity_selected(n->ptr);
		ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
		if (ImGui::Selectable("##selectednode", item_is_selected, selectable_flags, ImVec2(0, 0))) {
			if (node_entity) {
				if (ImGui::GetIO().KeyShift)
					ed_doc.do_mouse_selection(EditorDoc::MouseSelectionAction::ADD_SELECT, node_entity, false);
				else
					ed_doc.do_mouse_selection(EditorDoc::MouseSelectionAction::SELECT_ONLY, node_entity, false);
			}
			else
				ed_doc.selection_state->clear_all_selected();
		}

		if (ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[1] && node_entity) {
			ImGui::OpenPopup("outliner_ctx_menu");
			ed_doc.selection_state->add_to_entity_selection(n->ptr);
			oo->contextMenuHandle = n->ptr;
			ASSERT(n->ptr.get() == node_entity);
		}
		if (ImGui::BeginPopup("outliner_ctx_menu")) {

			if (oo->contextMenuHandle.get() == nullptr) {
				oo->contextMenuHandle = EntityPtr(nullptr);
				ImGui::CloseCurrentPopup();
			}
			else {

				Entity* const context_menu_entity = oo->contextMenuHandle.get();

				auto parent_to_shared = [&](Entity* me, bool create_new_parent) {
					auto& ents = ed_doc.selection_state->get_selection();
					std::vector<Entity*> ptrs;
					for (auto& ehandle : ents) {
						EntityPtr ptr(ehandle);
						if (ptr.get() == me) continue;
						ptrs.push_back(ptr.get());
					}
					ed_doc.command_mgr->add_command(new ParentToCommand(ed_doc, ptrs, me, create_new_parent, false));

					oo->contextMenuHandle = EntityPtr(nullptr);
				};
				auto remove_parent_of_selection = [&](bool delete_parent) {

					auto& ents = ed_doc.selection_state->get_selection();
					std::vector<Entity*> ptrs;
					for (auto& ehandle : ents) {
						EntityPtr ptr(ehandle);
						ptrs.push_back(ptr.get());
					}

					ed_doc.command_mgr->add_command(new ParentToCommand(ed_doc, ptrs, nullptr, false, delete_parent));

					oo->contextMenuHandle = EntityPtr(nullptr);
				};

				if (ImGui::MenuItem("Parent To This")) {
					parent_to_shared(context_menu_entity, false);
					ImGui::CloseCurrentPopup();
				}

				if (ImGui::MenuItem("Remove Parent")) {
					remove_parent_of_selection(false);
					ImGui::CloseCurrentPopup();
				}
				if (ImGui::MenuItem("Parent Selection To New Entity")) {
					parent_to_shared(nullptr, true);
					ImGui::CloseCurrentPopup();
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Add sibling entity")) {
					ed_doc.command_mgr->add_command(new CreateCppClassCommand(ed_doc, "Entity", context_menu_entity->get_ws_transform(), EntityPtr(context_menu_entity->get_parent()), false));
					oo->contextMenuHandle = EntityPtr(nullptr);
					ImGui::CloseCurrentPopup();
				}
				if (ImGui::MenuItem("Add child entity")) {
					ed_doc.command_mgr->add_command(new CreateCppClassCommand(ed_doc, "Entity", glm::mat4(1), context_menu_entity->get_self_ptr(), false));
					oo->contextMenuHandle = EntityPtr(nullptr);
					ImGui::CloseCurrentPopup();
				}

				if (context_menu_entity->get_parent()) {
					ImGui::Separator();

					bool make_cmd = false;
					MovePositionInHierarchy::Cmd c{};
					if (ImGui::MenuItem("Move next")) {
						make_cmd = true;
						c = MovePositionInHierarchy::Cmd::Next;
					}
					if (ImGui::MenuItem("Move prev")) {
						make_cmd = true;
						c = MovePositionInHierarchy::Cmd::Prev;
					}
					if (ImGui::MenuItem("Move first")) {
						make_cmd = true;
						c = MovePositionInHierarchy::Cmd::First;
					}
					if (ImGui::MenuItem("Move last")) {
						make_cmd = true;
						c = MovePositionInHierarchy::Cmd::Last;
					}

					if (make_cmd) {
						ed_doc.command_mgr->add_command(new MovePositionInHierarchy(ed_doc, context_menu_entity, c));
						ImGui::CloseCurrentPopup();
					}
				}

				ImGui::Separator();

				const bool is_entity_root_of_prefab = context_menu_entity && context_menu_entity->what_prefab && context_menu_entity->is_root_of_prefab;
				if (is_entity_root_of_prefab) {
					if (ImGui::MenuItem("Select prefab in browser")) {
						global_asset_browser.set_selected(context_menu_entity->what_prefab->get_name());
						ImGui::CloseCurrentPopup();
					}
					ImGui::Separator();
				}

				if (ImGui::MenuItem("Instantiate prefab", nullptr, nullptr, is_entity_root_of_prefab)) {
					ed_doc.command_mgr->add_command(new InstantiatePrefabCommand(ed_doc, context_menu_entity));
					oo->contextMenuHandle = EntityPtr(nullptr);
					ImGui::CloseCurrentPopup();
				}

				const bool branch_as_prefab_enabled = ed_doc.selection_state->num_entities_selected() == 1;
				if (ImGui::MenuItem("Save branch as prefab", nullptr, nullptr, branch_as_prefab_enabled)) {
					save_off_branch_as_scene(ed_doc, context_menu_entity);
					ImGui::CloseCurrentPopup();
				}

				auto is_prefab_instance_root = [&ed_doc](const Entity* e) -> bool {
					return e && e->is_root_of_prefab && e->what_prefab && e->what_prefab != ed_doc.get_editing_prefab();
				};

				if (is_prefab_instance_root(context_menu_entity)) {
					ImGui::Separator();
					if (context_menu_entity->get_prefab_editable()) {
						ImGui::PushStyleColor(ImGuiCol_Text, color32_to_imvec4({ 255,50,50,255 }));
						if (ImGui::MenuItem("Make Prefab Not Editable")) {
							ed_doc.command_mgr->add_command(new MakePrefabEditable(ed_doc, context_menu_entity, false));
						}
					}
					else {
						ImGui::PushStyleColor(ImGuiCol_Text, color32_to_imvec4({ 10,110,255,255 }));
						if (ImGui::MenuItem("Make Prefab Editable")) {
							ed_doc.command_mgr->add_command(new MakePrefabEditable(ed_doc, context_menu_entity, true));
						}
					}
					ImGui::PopStyleColor(1);
				}

				ImGui::Separator();
				ImGui::PushStyleColor(ImGuiCol_Text, color32_to_imvec4({ 255,50,50,255 }));
				if (ImGui::MenuItem("Dissolve As Parent")) {
					ASSERT(context_menu_entity);
					auto& children = context_menu_entity->get_children();

					if (!children.empty())
						remove_parent_of_selection(true);
					else
						ed_doc.command_mgr->add_command(new RemoveEntitiesCommand(ed_doc, { n->ptr }));

					ImGui::CloseCurrentPopup();
				}
				if (ImGui::MenuItem("Delete")) {
					ed_doc.command_mgr->add_command(new RemoveEntitiesCommand(ed_doc, { n->ptr }));
					ImGui::CloseCurrentPopup();
				}
				ImGui::PopStyleColor(1);

			}

			ImGui::EndPopup();
		}
	}

	ImGui::SameLine();

	if (!node_entity) {
		ImGui::Text("%s",ed_doc.get_name().c_str());
	}
	else {
		const Entity* const e = node_entity;

		const char* name = (e->get_editor_name().c_str());
		if (!*name) {
			if (e->is_root_of_prefab && e->what_prefab)
				name = e->what_prefab->get_name().c_str();
			else {
				if (auto m = e->get_component<MeshComponent>()) {
					if (m->get_model())
						name = m->get_model()->get_name().c_str();
				}
			}
		}
		if (!*name) {
			name = e->get_type().classname;
		}

		if (e->is_root_of_prefab && e->what_prefab != ed_doc.get_editing_prefab()) {
			const char* s = "eng/editor/prefab_p.png";
			auto tex = g_assets.find_global_sync<Texture>(s);
			if (tex) {
				ImGui::Image(ImTextureID(uint64_t(tex->gl_id)), ImVec2(tex->width, tex->height));
				ImGui::SameLine(0, 0);
			}
		}

		for (auto c : e->get_components()) {
			if (c->dont_serialize_or_edit_this()) continue;
			const char* s = c->get_editor_outliner_icon();
			if (!*s) continue;
			auto tex = g_assets.find_global_sync<Texture>(s);
			if (tex) {
				ImGui::Image(ImTextureID(uint64_t(tex->gl_id)), ImVec2(tex->width, tex->height));
				ImGui::SameLine(0, 0);
			}
		}

		if (!n->did_pass_filter) {
			ImGui::TextColored(ImVec4(0.3, 0.3, 0.3, 1),"%s", name);
		}
		else {
			if (!ed_doc.is_this_object_not_inherited(e))
				ImGui::TextColored(non_owner_source_color, "%s", name);
			else
				ImGui::Text("%s",name);
		}

	}

	ImGui::TableNextColumn();

	if (node_entity) {
		ImGui::TextColored(ImVec4(0.7, 0.7, 0.7, 1), "%lld", node_entity->get_instance_id());
	}
	ImGui::TableNextColumn();


	ImGui::PushStyleColor(ImGuiCol_Button, 0);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color32_to_imvec4({ 245, 242, 242, 55 }));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);

	{
		Entity* const e = node_entity;

		if (e) {
			auto img = (e->get_hidden_in_editor()) ? oo->hidden : oo->visible;
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0);
			if (ImGui::ImageButton(ImTextureID(uint64_t(img->gl_id)), ImVec2(16, 16))) {
				e->set_hidden_in_editor(!e->get_hidden_in_editor());
			}
		}
		else {
			auto img = oo->hidden;
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0);
			if (ImGui::ImageButton(ImTextureID(uint64_t(img->gl_id)), ImVec2(16, 16), ImVec2(), ImVec2(), -1, ImVec4(), ImVec4(0, 0, 0, 0))) {

			}
		}
	}
	ImGui::PopStyleColor(3);

	ImGui::PopID();
}

bool ObjectOutliner::should_draw_children(Entity* e) const
{
	return serialize_this_objects_children(e, ed_doc.get_editing_prefab());
}

int ObjectOutliner::determine_object_count() const
{
	return num_nodes + 1 /*root node*/;
}
void ObjectOutliner::draw()
{
	if (!ImGui::Begin("Outliner") || !rootnode) {
		ImGui::End();
		setScrollHere = EntityPtr();
		return;
	}

	filter->draw();

	int set_scroll_num = -1;
	if (setScrollHere.is_valid()) {
		IteratorDraw iter(this, rootnode.get());
		int current_iter_n = 0;
		do {
			assert(iter.get_node());
			auto the_ptr = iter.get_node()->ptr;
			if (the_ptr.handle != 0 && the_ptr == setScrollHere) {
				break;
			}
			current_iter_n++;
		} while (iter.step());

		set_scroll_num = current_iter_n;
	}

	ImGuiListClipper clipper;
	clipper.Begin(determine_object_count());
	IteratorDraw iter(this, rootnode.get());
	int cur_n = 0;
	ImGuiTableFlags const flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
	//if (ImGui::Begin("PropEdit")) {
	if (ImGui::BeginTable("Table", 3, flags)) {
		ImGui::TableSetupColumn("##Editor", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("##InstID", ImGuiTableColumnFlags_WidthFixed, 50.0);
		ImGui::TableSetupColumn("##Reset", ImGuiTableColumnFlags_WidthFixed, 50.0);

		while (clipper.Step()) {
			while (cur_n < clipper.DisplayStart) {
				iter.step();
				cur_n++;
			}
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
				iter.draw(ed_doc);
				iter.step();

				// dont set the scroll to an item already in view
				if (set_scroll_num == cur_n)
					set_scroll_num = -1;

				cur_n++;
			}
		}

		if (set_scroll_num != -1) {
			ImGui::SetScrollY(set_scroll_num * clipper.ItemsHeight);
		}

		ImGui::EndTable();
	}
	clipper.End();
	ImGui::End();

	setScrollHere = EntityPtr();
}


static void save_off_branch_as_scene(EditorDoc& ed_doc, Entity* e)
{
	auto serialize_branch = [&ed_doc](Entity* e) -> auto {
		PrefabAsset dummy;
		std::vector<Entity*> ents;
		ents.push_back(e);
		ed_doc.validate_fileids_before_serialize();
		return std::make_unique<SerializedSceneFile>(serialize_entities_to_text(ents, &dummy));
	};

	// vars
	bool failed = false;
	std::string selected_name;
	EntityPtr ptr(e);
	EditorPopupManager::inst->add_popup(
		"Enter name for new prefab: ",
		[ptr, serialize_branch, failed, selected_name]() mutable -> bool {
			Entity* e = ptr.get();
			if (!e) {
				sys_print(Error, "in prefab popup: entity was null\n");
				return true;	// close popup
			}

			std_string_input_text("##input", selected_name, 0);

			if (failed) {
				ImGui::TextColored(ImVec4(1, 0.1, 0.1, 1), "path already exists, try another.");
			}
			if (ImGui::Button("Save")) {
				// try file path
				std::string finalpath = selected_name.c_str();	// because imgui messes withstuff
				finalpath += ".pfb";

				const bool check_file_exists = FileSys::does_file_exist(finalpath.c_str(), FileSys::GAME_DIR);
				if (check_file_exists) {
					failed = true;
					return false;	// try again later
				}

				auto serialized = serialize_branch(e);

				auto outfile = FileSys::open_write_game(finalpath);
				outfile->write(serialized->text.c_str(), serialized->text.size());
				const char* str = string_format("saved prefab as: %s\n", finalpath.c_str());
				eng->log_to_fullscreen_gui(Info, str);
				sys_print(Info, str);

				return true;
			}
			ImGui::SameLine(0, 20);
			if (ImGui::Button("Cancel")) {
				return true;
			}

			return false;
		}
	);
}



static void HelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip())
	{
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void OONameFilter::draw()
{
	if (std_string_input_text("##filter", filter_component, ImGuiInputTextFlags_EnterReturnsTrue)) {
		filter_component = filter_component.c_str();
		on_filter_enter.invoke(filter_component);
	}
	ImGui::SameLine(); 
	HelpMarker(
		"Filter scene.\n"
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
		"Return to confirm filter.\n"
	);
}

// dumb but want something that just works
bool OONameFilter::is_in_string(const string& filter, const string& match)
{
	auto lowercased = to_lower(match);
	return lowercased.find(filter) != std::string::npos;
}

static bool check_props_for_assetptr_or_entityptr(const string& filter, void* inst, const PropertyInfoList* list)
{
	for (int i = 0; i < list->count; i++) {
		auto& prop = list->list[i];
		if (strcmp("AssetPtr", prop.custom_type_str) == 0)
		{
			IAsset** asset = (IAsset**)prop.get_ptr(inst);
			if (*asset) {
				if (OONameFilter::is_in_string(filter, (*asset)->get_name()))	// asset name
					return true;
			}
		}
		else if (strcmp("ObjPtr", prop.custom_type_str) == 0) {
			obj<BaseUpdater>* eptr = (obj<BaseUpdater>*)prop.get_ptr(inst);
			BaseUpdater* what = eptr->get();
			if (what && OONameFilter::is_in_string(filter, std::to_string(what->get_instance_id())))	// entity ptr instance id
				return true;
		}
		else if (prop.type == core_type_id::List) {
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

bool OONameFilter::does_entity_pass_one_filter(const string& filter, Entity* e)
{
	// check: name of object, component names, id of object
	// props: asset names, ptr names/ids
	if (is_in_string(filter, e->get_editor_name()))	// name of object
		return true;
	for (auto& c : e->get_components()) {
		if (is_in_string(filter, c->get_type().classname))	// names of components
			return true;
	}
	if (e->what_prefab && is_in_string(filter, e->what_prefab->get_name()))	// prefab name
		return true;
	if (is_in_string(filter, std::to_string(e->get_instance_id())))	// instance id
		return true;
	
	// now check properties
	for (auto& c : e->get_components()) {
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

	return false;	// no matches
}

bool OONameFilter::does_entity_pass(const vector<vector<string>>& filter, Entity* e)
{
	if (filter.empty())
		return true;
	for (auto& ors : filter) {
		bool res = true;
		for (auto& ands : ors) {
			if (!does_entity_pass_one_filter(ands,e)) {
				res = false;
				break;
			}
		}
		if (res)
			return true;
	}
	return false;
}
#include <sstream>

vector<vector<string>> OONameFilter::parse_into_and_ors(const std::string& filter)
{
	std::vector<std::vector<std::string>> result;
	std::stringstream ss(filter);
	std::string segment;

	while (std::getline(ss, segment, '|')) {  // Split by '|'
		std::vector<std::string> subVector;
		std::stringstream subSS(segment);
		std::string word;

		while (subSS >> word) {  // Split by spaces within segments
			subVector.push_back(to_lower(word));
		}

		result.push_back(subVector);
	}

	return result;

}