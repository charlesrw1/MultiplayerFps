// IteratorDraw::draw — per-row rendering of the outliner table.
// This file is inside a #if 0 block (disabled/legacy code).
#ifdef EDITOR_BUILD
#include "EditorDocLocal.h"
#include "Commands.h"
#include "Assets/AssetDatabase.h"
#include "EditorPopups.h"
#include "Framework/Files.h"
#include "Framework/MyImguiLib.h"
#include "ObjectOutlineFilter.h"

#if 0

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

static void save_off_branch_as_scene(EditorDoc& ed_doc, Entity* e);

void ObjectOutliner::IteratorDraw::draw(EditorDoc& ed_doc)
{
	ASSERT(node != nullptr);

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
			auto tex = g_assets.find<Texture>(s);
			if (tex) {
				my_imgui_image(tex, -1);
				ImGui::SameLine(0, 0);
			}
		}

		for (auto c : e->get_components()) {
			if (c->dont_serialize_or_edit_this()) continue;
			const char* s = c->get_editor_outliner_icon();
			if (!*s) continue;
			auto tex = g_assets.find<Texture>(s);
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

#endif
#endif
