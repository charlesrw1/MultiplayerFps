#include "EditorDocLocal.h"

#include "Framework/StringUtils.h"
#include "ObjectOutlineFilter.h"
#include "Animation/SkeletonData.h"

#include "Framework/SerializerJson2.h"

static bool check_if_string_is_number(const char* str) {
	try {
		int dummy = std::stod(str);
		return true;
	}
	catch (...) {
		return false;
	}
}

void EditorDoc::add_editor_commands() {
	cmds->add("all", [this](const Cmd_Args& args) {
		auto& objs = eng->get_level()->get_all_objects();
		for (auto s : objs)
			args.sys_print(Info, "%lld\n", int64_t(s->get_instance_id()));
	});
	cmds->add("sel", [this](const Cmd_Args& args) {
		auto selected = selection_state->get_selection_as_vector();
		for (auto s : selected)
			args.sys_print(Info, "%lld\n", int64_t(s.handle));
	});
	cmds->add("pp", [this](const Cmd_Args& args) {
		const string ents = args.at(1);
		auto lines = StringUtils::to_lines(ents);

		for (auto& e : lines) {
			int64_t id = std::stoll(e);
			Entity* ent = EntityPtr(id).get();
			if (!ent)
				continue;
			const char* comp_name = "<no component>";
			string ent_name = get_name_display_entity(ent);

			args.sys_print(Info, "%-5lld %-20s %s\n", int64_t(id), comp_name, ent_name.c_str());
		}
	});

	cmds->add("fil", [this](const Cmd_Args& args) {
		const string ents = args.at(2);
		const string filter = args.at(1);

		auto lines = StringUtils::to_lines(ents);
		for (auto& e : lines) {
			int64_t id = std::stoll(e);
			Entity* ent = EntityPtr(id).get();
			if (!ent)
				continue;
			if (OONameFilter::does_entity_pass_one_filter(filter, ent))
				args.sys_print(Info, "%lld\n", int64_t(id));
		}
	});
	cmds->add("so", [this](const Cmd_Args& args) {
		if (args.size() == 1) {
			selection_state->clear_all_selected();
			return;
		}
		const string ents = args.at(1);
		auto lines = StringUtils::to_lines(ents);
		selection_state->clear_all_selected();
		for (auto& e : lines) {
			int64_t id = std::stoll(e);
			selection_state->add_to_entity_selection(EntityPtr(id));
		}
	});
	cmds->add("sfso", [this](const Cmd_Args& args) {
		auto str = string_format("sel | fil %s | so\n", args.at(1));
		Cmd_Manager::inst->execute(Cmd_Execute_Mode::NOW, str);
	});
	cmds->add("afso", [this](const Cmd_Args& args) {
		auto str = string_format("all | fil %s | so\n", args.at(1));
		Cmd_Manager::inst->execute(Cmd_Execute_Mode::NOW, str);
	});

	cmds->add("as", [this](const Cmd_Args& args) {
		const string ents = args.at(1);
		auto lines = StringUtils::to_lines(ents);
		for (auto& e : lines) {
			int64_t id = std::stoll(e);
			selection_state->add_to_entity_selection(EntityPtr(id));
		}
	});
	cmds->add("us", [this](const Cmd_Args& args) {
		const string ents = args.at(1);
		auto lines = StringUtils::to_lines(ents);
		for (auto& e : lines) {
			int64_t id = std::stoll(e);
			selection_state->remove_from_selection(EntityPtr(id));
		}
	});
	cmds->add("set-to-camera", [this](const Cmd_Args& args) {
		if (ed_cam.get_is_using_ortho()) {
			return;
		}
		if (!selection_state->has_only_one_selected())
			return;
		auto ent = selection_state->get_only_one_selected();
		if (ent.get()) {
			glm::mat4 cam_transform = glm::inverse(vs_setup.view);
			ent->set_ws_transform(cam_transform);
			manipulate->update_pivot_and_cached();
		}
	});
	cmds->add("set-field", [this](const Cmd_Args& args) {
		const string ents = args.at(3);
		nlohmann::json jsonObj;
		if (check_if_string_is_number(args.at(2)))
			jsonObj[args.at(1)] = std::stod(args.at(2));
		else if (strcmp(args.at(2), "true") == 0)
			jsonObj[args.at(1)] = true;
		else if (strcmp(args.at(2), "false") == 0)
			jsonObj[args.at(1)] = false;
		else
			jsonObj[args.at(1)] = args.at(2);

		const string filter = args.at(1);

		auto lines = StringUtils::to_lines(ents);
		for (auto& e : lines) {
			int64_t id = std::stoll(e);
			Entity* ent = EntityPtr(id).get();
			if (!ent)
				continue;
			{ ReadSerializerBackendJson2 reader("", jsonObj, *ent); }
			if (ent->get_components().size() > 0) {
				ReadSerializerBackendJson2 reader("", jsonObj, *ent->get_components().at(0));
			}
			ent->invalidate_transform(nullptr);
		}
	});

	cmds->add("set-box", [this](const Cmd_Args& args) {
		glm::vec3 min{};
		glm::vec3 max{};
		for (int i = 0; i < 3; i++)
			min[i] = std::atof(args.at(i + 1));
		for (int i = 0; i < 3; i++)
			max[i] = std::atof(args.at(i + 4));

		for (int i = 0; i < 3; i++) {
			if (min[i] > max[i])
				std::swap(min[i], max[i]);
		}
		if (!selection_state->has_only_one_selected())
			return;
		Entity* e = selection_state->get_only_one_selected().get();
		if (!e)
			return;
		MeshComponent* mc = e->get_component<MeshComponent>();
		if (!mc || !mc->get_model())
			return;
		auto bounds = mc->get_model()->get_bounds();

		glm::vec3 set_size = (max - min);
		vec3 bounds_size = bounds.bmax - bounds.bmin;

		glm::vec3 scale = set_size / bounds_size;
		glm::vec3 bounds_c = bounds.get_center();

		glm::vec3 frac = glm::abs(bounds.bmin / bounds_size);

		glm::vec3 set_center = frac * set_size + min;
		e->set_ws_transform_comp(set_center, glm::quat(), scale);
	});
	cmds->add("bone-list", [this](const Cmd_Args& args) {
		if (!selection_state->has_only_one_selected())
			return;
		Entity* e = selection_state->get_only_one_selected().get();
		if (!e)
			return;
		MeshComponent* mc = e->get_component<MeshComponent>();
		if (!mc || !mc->get_model())
			return;
		MSkeleton* skel = mc->get_model()->get_skel();
		if (!skel)
			return;
		auto& bones = skel->get_all_bones();
		int count = bones.size();
		sys_print(Info, "%s: num bones: %d\n", mc->get_model()->get_name().c_str(), count);
		for (int i = 0; i < count; i++) {
			sys_print(Info, "\t%s\n", bones[i].strname.c_str());
		}
	});
	cmds->add("parent-to", [this](const Cmd_Args& args) {
		if (!selection_state->has_only_one_selected())
			return;
		Entity* e = selection_state->get_only_one_selected().get();
		if (!e)
			return;
		int64_t id = std::stoll(args.at(1));
		string bonename = args.at(2);
		EntityPtr parent_to(id);
		Entity* parent_to_e = parent_to.get();
		if (!parent_to_e)
			return;
		e->parent_to(parent_to_e);
		e->set_ls_position(glm::vec3(0.f));
		e->set_parent_bone(bonename.c_str());
	});

	cmds->add("SET_ORBIT_TARGET", [this](const Cmd_Args&) { set_camera_target_to_sel(); });
	cmds->add("ed.HideSelected", [this](const Cmd_Args&) {
		eng->log_to_fullscreen_gui(Info, "Hide selected");
		auto& selection = selection_state->get_selection();
		for (auto s : selection) {
			EntityPtr handle(s);
			if (handle) {
				handle->set_hidden_in_editor(true);
			}
		}
	});
	cmds->add("ed.UnHideAll", [this](const Cmd_Args&) {
		eng->log_to_fullscreen_gui(Info, "Unhide all");
		auto level = eng->get_level();
		if (level) {
			for (auto e : level->get_all_objects()) {
				if (e->is_a<Entity>()) {
					auto ent = e->cast_to<Entity>();
					if (ent->get_hidden_in_editor())
						ent->set_hidden_in_editor(false);
				}
			}
		}
	});
	cmds->add("make-prefab-and-replace", [this](const Cmd_Args& args) {
		if (args.size() < 2) {
			args.sys_print(Warning, "Usage: make-prefab-and-replace <prefab_path>\n");
			return;
		}
		if (is_editing_prefab()) {
			args.sys_print(Warning,
						   "make-prefab-and-replace: refused in prefab edit mode (would spawn a "
						   "PrefabAssetComponent reference inside a prefab)\n");
			return;
		}
		auto selection = selection_state->get_selection_as_vector();
		if (selection.empty()) {
			args.sys_print(Warning, "No entities selected\n");
			return;
		}
		std::string prefab_path = args.at(1);
		auto cmd = new MakePrefabAndReplaceCommand(*this, selection, prefab_path);
		command_mgr->add_command(cmd);
	});
}