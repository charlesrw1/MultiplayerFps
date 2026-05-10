// Object Outliner: tree construction, selection, iterator step, and main draw loop.
// Context menus -> ObjectOutlinerContextMenu.cpp
// Per-row rendering -> ObjectOutlinerDraw.cpp
// Filter/search -> ObjectOutlinerFilter.cpp
#ifdef EDITOR_BUILD
#include "EditorDocLocal.h"
#include "Commands.h"
#include "Assets/AssetDatabase.h"
#include "EditorPopups.h"
#include "Framework/Files.h"
#include "Framework/MyImguiLib.h"
#include "ObjectOutlineFilter.h"

#if 0
ObjectOutliner::~ObjectOutliner() {

}

void ObjectOutliner::rebuild_tree() {
	delete_tree();

	auto the_filter = OONameFilter::parse_into_and_ors(filter->get_filter());

	rootnode = std::make_unique<Node>();

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
	ASSERT(a != nullptr);
	ASSERT(b != nullptr);

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
	ASSERT(oo != nullptr);
	ASSERT(initfrom != nullptr);
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
	ASSERT(ed_doc.selection_state != nullptr);
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
	ASSERT(node != nullptr);
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

bool ObjectOutliner::should_draw_this(Entity* e) const
{
	ASSERT(e != nullptr);
	return this_is_a_serializeable_object(e);
}

int ObjectOutliner::determine_object_count() const
{
	ASSERT(num_nodes >= 0);
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

#endif

static void save_off_branch_as_scene(EditorDoc& ed_doc, Entity* e) {
	ASSERT(e != nullptr);
	auto serialize_branch = [&ed_doc](Entity * e) -> auto {
		assert(!"not implmented");
		std::vector<Entity*> ents;
		ents.push_back(e);
		ed_doc.validate_fileids_before_serialize();
		return nullptr; // std::make_unique<SerializedSceneFile>(serialize_entities_to_text("save_branch", ents));
	};

	// vars
	bool failed = false;
	std::string selected_name;
	EntityPtr ptr(e);
	EditorPopupManager::inst->add_popup(
		"Enter name for new prefab: ", [ptr, serialize_branch, failed, selected_name]() mutable -> bool {
			Entity* e = ptr.get();
			if (!e) {
				sys_print(Error, "in prefab popup: entity was null\n");
				return true; // close popup
			}

			std_string_input_text("##input", selected_name, 0);

			if (failed) {
				ImGui::TextColored(ImVec4(1, 0.1, 0.1, 1), "path already exists, try another.");
			}
			if (ImGui::Button("Save")) {
				// try file path
				std::string finalpath = selected_name.c_str(); // because imgui messes withstuff
				finalpath += ".pfb";

				const bool check_file_exists = FileSys::does_file_exist(finalpath.c_str(), FileSys::GAME_DIR);
				if (check_file_exists) {
					failed = true;
					return false; // try again later
				}

				// auto serialized = serialize_branch(e);
				//
				// auto outfile = FileSys::open_write_game(finalpath);
				// outfile->write(serialized->text.c_str(), serialized->text.size());
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
		});
}
#endif
