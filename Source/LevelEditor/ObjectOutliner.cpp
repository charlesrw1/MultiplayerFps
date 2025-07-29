#ifdef EDITOR_BUILD
#include "EditorDocLocal.h"
#include "Commands.h"
#include "Assets/AssetDatabase.h"
#include "EditorPopups.h"
#include "Framework/Files.h"
#include "Framework/MyImguiLib.h"
#include "ObjectOutlineFilter.h"
#include "EditorFolderComponent.h"
ObjectOutliner::~ObjectOutliner() {

}

void ObjectOutliner::rebuild_tree() {
	delete_tree();

	auto the_filter = OONameFilter::parse_into_and_ors(filter->get_filter());

	rootnode = std::make_unique<Node>();

	auto folder = (EditorMapDataComponent*)eng->get_level()->find_first_component(&EditorMapDataComponent::StaticType);
	if (!folder) {
		auto newEnt = ed_doc.spawn_entity();
		folder = (EditorMapDataComponent*)ed_doc.attach_component(&EditorMapDataComponent::StaticType, newEnt);
	}
	assert(folder);
	cachedContainer = folder;

	std::unordered_map<int, Node*> folderNodes;
	for (auto& f : folder->get_folders()) {
		auto folderNode = std::make_unique<Node>();
		folderNode->is_folder = true;
		folderNode->folderid = f.id;
		folderNode->is_folder_open = f.is_folder_open;
		folderNodes.insert({ int(f.id),folderNode.get() });
		rootnode->add_child(std::move(folderNode));
		num_nodes++;	// badbadbad
	}

	auto level = eng->get_level();
	auto& all_objs = level->get_all_objects();
	for (auto ent : all_objs) {
		if (Entity* e = ent->cast_to<Entity>()) {
			if (!e->get_parent() && should_draw_this(e)) {

				int8_t folderId = e->editor_folder;
				auto addNodeToThis = rootnode.get();

				if (folderId != 0) {
					auto findFolder = MapUtil::get_or(folderNodes, int(folderId), (Node*)nullptr);
					if (!findFolder) {
						e->editor_folder = 0;
					}
					else {
						// skip if closed and filter empty
						if (!findFolder->is_folder_open && the_filter.empty())
							continue;

						addNodeToThis = findFolder;
					}
				}
				addNodeToThis->add_child(std::make_unique<Node>(this, e, the_filter));	
				if (!addNodeToThis->children.back()->is_visible) {
					addNodeToThis->children.pop_back();
					num_nodes--;
				}
			}
		}
	}
	rootnode->sort_children();

}
void ObjectOutliner::do_recursive_select(Entity* a, Entity* b) {

	auto recurse = [](auto&& self, Node* node, bool found_yet, Entity* a, Entity* b, EditorDoc& ed_doc) -> bool {

		if (node->ptr.get() == a || node->ptr.get() == b) {
			found_yet = !found_yet;
			ed_doc.selection_state->add_to_entity_selection(node->ptr);
		}
		else if (found_yet && node->ptr.get()) {
			ed_doc.selection_state->add_to_entity_selection(node->ptr);
		}

		for (auto& c : node->children) {
			found_yet = self(self, c.get(), found_yet, a, b, ed_doc);
		}
		return found_yet;
	};
	if (rootnode.get()) {
		recurse(recurse, rootnode.get(), false, a, b, ed_doc);
	}
}
ObjectOutliner::Node::Node(ObjectOutliner* oo, Entity* initfrom, const std::vector<std::vector<std::string>>& filter) {
	did_pass_filter = OONameFilter::does_entity_pass(filter, initfrom);
	assert(oo->should_draw_this(initfrom));
	ptr = initfrom->get_self_ptr();
	assert(ptr.get());
	auto& children = initfrom->get_children();
	bool do_any_children_pass = false;

	for (auto& c : children) {
		if (oo->should_draw_this(c)) {
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

	folderOpen = g_assets.find_global_sync<Texture>("eng/editor/folder_open.png").get();
	folderClosed = g_assets.find_global_sync<Texture>("eng/editor/folder_closed.png").get();
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

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;


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
	//assert(node_entity || !node->parent);

	const bool is_root_node = node_entity == nullptr;

	auto get_folder = [&]() -> EditorFolder* {
		auto getContainer = oo->cachedContainer.get();
		if (!getContainer)
			return nullptr;
		return getContainer->lookup_for_id(n->folderid);
	};


	ImGui::PushID(n);
	{
		const bool item_is_selected = ed_doc.selection_state->is_entity_selected(n->ptr);
		ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
		if (ImGui::Selectable("##selectednode", item_is_selected, selectable_flags, ImVec2(0, 0))) {
			if (node_entity) {
				const bool has_selection_already = ed_doc.selection_state->has_any_selected();

				if (ImGui::GetIO().KeyShift && has_selection_already) {
					auto vec = ed_doc.selection_state->get_selection_as_vector();
					auto first = vec.at(0);
					oo->do_recursive_select(first.get(), node_entity);
				}
				else if (ImGui::GetIO().KeyCtrl) {
					ed_doc.do_mouse_selection(MouseSelectionAction::ADD_SELECT, node_entity, false);
				}
				else {
					ed_doc.do_mouse_selection(MouseSelectionAction::SELECT_ONLY, node_entity, false);
				}
			}
			else if (n->is_folder) {
				auto fObj = get_folder();
				if (fObj) {
					auto do_selection = [&]() {
						auto& objs = eng->get_level()->get_all_objects();
						std::vector<EntityPtr> selectThese;
						for (auto obj : objs) {
							if (auto as_ent = obj->cast_to<Entity>()) {
								if (as_ent->get_folder_id() == fObj->id)
									selectThese.push_back(as_ent);
							}
						}
						ed_doc.selection_state->add_entities_to_selection(selectThese);
					};

					if (ImGui::GetIO().KeyCtrl) {
						do_selection();
					}
					else {
						ed_doc.selection_state->clear_all_selected();
						do_selection();
					}
				}
			}
			else {
				ed_doc.selection_state->clear_all_selected();
			}
		}

		if (ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[1] && (n->ptr||n->is_folder)) {
			ImGui::OpenPopup("outliner_ctx_menu");
			if(node_entity) {
				ed_doc.selection_state->add_to_entity_selection(n->ptr);
				oo->contextMenuHandle = n->ptr;
				ASSERT(n->ptr.get() == node_entity);
			}
			else {
				oo->contextMenuHandle = FolderId{ n->folderid };
			}
		}
		if (ImGui::BeginPopup("outliner_ctx_menu")) {

			std::visit(overloaded{
				[&](EntityPtr ptr) {this->draw_entity_context_menu(ptr,ed_doc); },
				[&](FolderId folder) { this->draw_folder_context_menu(folder,ed_doc); },
				[](std::monostate) {  }
				}, oo->contextMenuHandle);

			ImGui::EndPopup();
		}
	}


	ImGui::SameLine();
	if (n->is_folder) {
		auto getContainer = oo->cachedContainer.get();
		bool print_bad = true;
		if (getContainer) {
			auto folderObj = getContainer->lookup_for_id(n->folderid);
			if (folderObj) {

				auto tex = (folderObj->is_folder_open) ? oo->folderOpen : oo->folderClosed;
				if (tex) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
					if (my_imgui_image_button(tex, tex->get_size().x)) {
						folderObj->is_folder_open = !folderObj->is_folder_open;
						oo->refresh_flag = true;	// refresh after
					}
					ImGui::PopStyleColor();
					ImGui::SameLine(0, 5);
				}

				auto& name = folderObj->folderName;
				ImGui::TextColored(ImColor(240,165,60), "%s\n", name.c_str());
				print_bad = false;
			}
		}
		if (print_bad) {
			ImGui::Text("<bad folder>\n");
		}
	}
	else if (!node_entity) {
		ImGui::Text("%s",ed_doc.get_name().c_str());
	}
	else {
		const Entity* const e = node_entity;
		const bool is_prefab_root = e->get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab;
		const char* name = (e->get_editor_name().c_str());
		if (!*name) {
			if (is_prefab_root)
				name = e->get_object_prefab().get_name().c_str();
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

		if (is_prefab_root) {
			const char* s = "eng/editor/prefab_p.png";
			auto tex = g_assets.find_global_sync<Texture>(s);
			if (tex) {
				my_imgui_image(tex, -1);
				ImGui::SameLine(0, 0);
			}
		}

		for (auto c : e->get_components()) {
			if (c->dont_serialize_or_edit_this()) continue;
			const char* s = c->get_editor_outliner_icon();
			if (!*s) continue;
			auto tex = g_assets.find_global_sync<Texture>(s);
			if (tex) {
				my_imgui_image(tex, -1);
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
		ImGui::TextColored(ImVec4(0.7, 0.7, 0.7, 1), "%d", node_entity->unique_file_id);
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
			if (my_imgui_image_button(img, 16)) {
				e->set_hidden_in_editor(!e->get_hidden_in_editor());
			}
		}
		else if (n->is_folder) {
			auto getContainer = oo->cachedContainer.get();
			if (getContainer) {
				auto folderObj = getContainer->lookup_for_id(n->folderid);
				if (folderObj) {
					auto img = (folderObj->is_hidden) ? oo->hidden : oo->visible;
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0);
					if (my_imgui_image_button(img, 16)) {
						folderObj->is_hidden = !folderObj->is_hidden;
						for (auto obj : eng->get_level()->get_all_objects()) {
							if (auto as_ent = obj->cast_to<Entity>()) {
								if(as_ent->get_folder_id()==folderObj->id)
									as_ent->set_hidden_in_editor(folderObj->is_hidden);
							}
						}
					}
				}
			}
		}
		else {
			auto img = oo->hidden;
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0);
			if (ImGui::ImageButton(ImTextureID(uint64_t(img->get_internal_render_handle())), ImVec2(16, 16), ImVec2(), ImVec2(), -1, ImVec4(), ImVec4(0, 0, 0, 0))) {

			}
		}
	}
	ImGui::PopStyleColor(3);

	ImGui::PopID();
}

void ObjectOutliner::IteratorDraw::draw_folder_context_menu(ObjectOutliner::FolderId folder, EditorDoc& ed_doc) {
	auto container = oo->cachedContainer.get();
	if (!container) {
		oo->contextMenuHandle = std::monostate();// EntityPtr(nullptr);
		ImGui::CloseCurrentPopup();
		return;
	}
	auto fObj = container->lookup_for_id(folder.id);
	if (!fObj) {
		oo->contextMenuHandle = std::monostate();// EntityPtr(nullptr);
		ImGui::CloseCurrentPopup();
		return;
	}

	if (ImGui::MenuItem("Set Folder")) {
		//SetFolderCommand
		auto& ents = ed_doc.selection_state->get_selection();
		std::vector<EntityPtr> ptrs;
		for (auto& ehandle : ents) {
			EntityPtr ptr(ehandle);
			ptrs.push_back(ptr);
		}
		ed_doc.command_mgr->add_command(new SetFolderCommand(ed_doc, ptrs,fObj->id));

		oo->contextMenuHandle = std::monostate();
		ImGui::CloseCurrentPopup();
		return;
	}
	if (ImGui::MenuItem("Select Objects")) {
		ed_doc.selection_state->clear_all_selected();
		auto& objs = eng->get_level()->get_all_objects();
		for (auto obj : objs) {
			if (auto as_ent = obj->cast_to<Entity>()) {
				if (as_ent->get_folder_id() == fObj->id)
					ed_doc.selection_state->add_to_entity_selection(as_ent);
			}
		}
		oo->contextMenuHandle = std::monostate();
		ImGui::CloseCurrentPopup();
	}
	ImGui::Separator();
	ImGui::PushStyleColor(ImGuiCol_Text, color32_to_imvec4({ 255,50,50,255 }));
	if (ImGui::MenuItem("Remove Folder")) {
		container->remove_folder(fObj->id);
		fObj = nullptr;
		oo->contextMenuHandle = std::monostate();
		ImGui::CloseCurrentPopup();
	}
	ImGui::PopStyleColor();
}

void ObjectOutliner::IteratorDraw::draw_entity_context_menu(EntityPtr ptr, EditorDoc& ed_doc) {
	Entity* const context_menu_entity = ptr.get();

	if (context_menu_entity == nullptr) {
		oo->contextMenuHandle = std::monostate();// EntityPtr(nullptr);
		ImGui::CloseCurrentPopup();
		return;
	}


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

	const bool is_entity_root_of_prefab = context_menu_entity && context_menu_entity->get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab;
	if (is_entity_root_of_prefab) {
		if (ImGui::MenuItem("Select prefab in browser")) {
			AssetBrowser::inst->set_selected(context_menu_entity->get_object_prefab().get_name());
			ImGui::CloseCurrentPopup();
		}
		ImGui::Separator();
	}


	const bool branch_as_prefab_enabled = ed_doc.selection_state->num_entities_selected() == 1;
	if (ImGui::MenuItem("Save branch as prefab", nullptr, nullptr, branch_as_prefab_enabled)) {
		save_off_branch_as_scene(ed_doc, context_menu_entity);
		ImGui::CloseCurrentPopup();
	}


	ImGui::Separator();
	ImGui::PushStyleColor(ImGuiCol_Text, color32_to_imvec4({ 255,50,50,255 }));
	if (ImGui::MenuItem("Dissolve As Parent")) {
		ASSERT(context_menu_entity);
		auto& children = context_menu_entity->get_children();

		if (!children.empty())
			remove_parent_of_selection(true);
		else
			ed_doc.command_mgr->add_command(new RemoveEntitiesCommand(ed_doc, { context_menu_entity }));

		ImGui::CloseCurrentPopup();
	}
	if (ImGui::MenuItem("Delete")) {
		ed_doc.command_mgr->add_command(new RemoveEntitiesCommand(ed_doc, { context_menu_entity }));
		ImGui::CloseCurrentPopup();
	}
	ImGui::PopStyleColor(1);
}

bool ObjectOutliner::should_draw_this(Entity* e) const
{
	return this_is_a_serializeable_object(e);
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

	if (refresh_flag) {
		rebuild_tree();
		refresh_flag = false;
	}
}


static void save_off_branch_as_scene(EditorDoc& ed_doc, Entity* e)
{
	auto serialize_branch = [&ed_doc](Entity* e) -> auto {
		assert(!"not implmented");
		std::vector<Entity*> ents;
		ents.push_back(e);
		ed_doc.validate_fileids_before_serialize();
		return std::make_unique<SerializedSceneFile>(serialize_entities_to_text("save_branch",ents));
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
		if (prop.type==core_type_id::AssetPtr)
		{
			IAsset** asset = (IAsset**)prop.get_ptr(inst);
			if (*asset) {
				if (OONameFilter::is_in_string(filter, (*asset)->get_name()))	// asset name
					return true;
			}
		}
		else if (prop.type==core_type_id::ObjHandlePtr) {
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
	if (e->get_object_prefab_spawn_type()==EntityPrefabSpawnType::RootOfPrefab && is_in_string(filter, e->get_object_prefab().get_name()))	// prefab name
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
#endif