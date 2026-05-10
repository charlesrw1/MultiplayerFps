// Context menu implementations for the Object Outliner (folder and entity menus).
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

static void save_off_branch_as_scene(EditorDoc& ed_doc, Entity* e);

void ObjectOutliner::IteratorDraw::draw_folder_context_menu(ObjectOutliner::FolderId folder, EditorDoc& ed_doc) {
	ASSERT(oo != nullptr);
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
	ASSERT(oo != nullptr);
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

#endif
#endif
