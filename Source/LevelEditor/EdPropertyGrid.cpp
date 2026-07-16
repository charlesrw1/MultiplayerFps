#include "EdPropertyGrid.h"
#include "Framework/Config.h"
#include "imgui.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Render/Texture.h"

#include "Assets/AssetRegistry.h"
#include "EditorDocLocal.h"
#include "Framework/MyImguiLib.h"
#include "Game/Components/SpawnerComponenth.h"

#include "Game/Components/SpawnerComponenth.h"
#include "PropertyEditors.h"
#include "Game/Components/MeshComponent.h"

class SpawnerIProped : public IPropertyEditor
{
public:
	SpawnerIProped(SpawnerComponent* sc, string key) : sc(sc), key2(key) {
		prop = &hacked_bullshit;
		hacked_bullshit.name = key2.c_str();
		value = sc->obj[key];
		instance = this; // bs
	}
	~SpawnerIProped() {
		if (sc.get())
			sc->obj[key2] = value;
	}
	bool internal_update() {
		ImguiInputTextCallbackUserStruct user;
		user.string = &value;
		if (ImGui::InputText("##input_text", (char*)value.c_str(), value.size() + 1 /* null terminator byte */,
							 ImGuiInputTextFlags_CallbackResize, imgui_input_text_callback_function, &user)) {
			value.resize(strlen(value.c_str())); // imgui messes with buffer size
			sc->obj[key2] = value;
			return true;
		}
		return false;
	}
	PropertyInfo hacked_bullshit;
	string key2;
	string value;
	obj<SpawnerComponent> sc;
};

class SpawnerModelProp : public IPropertyEditor
{
public:
	SpawnerModelProp(SpawnerComponent* sc) : sc(sc) {
		prop = &hacked_bullshit;
		hacked_bullshit.name = "model";

		instance = this; // bs

		assetprop.instance = this;
		assetprop.prop = &hacked_bullshit2;
		hacked_bullshit2.type = core_type_id::AssetPtr;
		hacked_bullshit2.class_type = &Model::StaticType;
		hacked_bullshit2.offset = offsetof(SpawnerModelProp, model);

		if (sc->obj["model"].is_string()) {
			string str = sc->obj["model"];
			if (!str.empty())
				model = Model::load(str);
		}
	}
	~SpawnerModelProp() {}
	bool internal_update() {
		bool res = assetprop.internal_update();
		if (res) {
			set_mod();
		}
		return res;
	}
	bool can_reset() final { return assetprop.can_reset(); }
	void reset_value() final {
		assetprop.reset_value();
		set_mod();
	}
	void set_mod() {
		if (model)
			sc->obj["model"] = model->get_name();
		else
			sc->obj["model"] = "";

		sc->set_model();
	}

	AssetPropertyEditor assetprop;
	Model* model = nullptr;

	PropertyInfo hacked_bullshit;
	PropertyInfo hacked_bullshit2;

	obj<SpawnerComponent> sc;
};

Component* EdPropertyGrid::get_selected_component(Entity* e) const {
	ASSERT(e);
	if (e->get_components().size() == 0) return nullptr;
	return e->get_components().at(0);
}

void EdPropertyGrid::draw_components(const ISelectionApi& api, Entity* entity) {

	
}

void EdPropertyGrid::draw(const ISelectionApi& api) {

	auto selected_vec = api.get_selected();


	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	if (ImGui::Begin("Properties")) {
		if (selected_vec.size() == 1) {
			auto* ent = selected_vec.at(0).get();
			if (ent) {
				auto* selected = get_selected_component(ent);
				if (selected && selected->is_a<SpawnerComponent>()) {
					auto sc = (SpawnerComponent*)selected;
					ImGui::Text("typename: %s\n", sc->get_spawner_type().c_str());
				}

				std::vector<obj<BaseUpdater>> what_i_want;
				what_i_want.push_back(ent);
				if(selected)
					what_i_want.push_back(selected);

				EntityPtr entPtr = ent->get_self_ptr();
				if (!property_edit_session_active)
					session_before_snapshot = std::shared_ptr<SerializedSceneFile>(
						CommandSerializeUtil::serialize_entities_text(ed_doc, {entPtr}));

				const bool changed = grid_cache.set_what_i_want_and_draw(what_i_want);

				if (changed) {
					property_edit_session_active = true;
					ed_doc.set_has_editor_changes();

					Entity* e = selected_vec.at(0).get();
					ASSERT(e);
					e->editor_on_change_properties();
					e->post_change_transform_R();

					auto ec = get_selected_component(selected_vec.at(0).get());
					if (ec)
						ec->editor_on_change_property();

					on_property_change_internal.invoke();
				}

				// A whole edit "session" (e.g. an entire slider drag) ends once ImGui reports
				// nothing is being interacted with; coalesce it into a single undoable command
				// instead of one per changed-frame.
				if (property_edit_session_active && !ImGui::IsAnyItemActive()) {
					std::shared_ptr<SerializedSceneFile> after_snapshot(
						CommandSerializeUtil::serialize_entities_text(ed_doc, {entPtr}));
					ed_doc.command_mgr->add_command(
						new SetEntityStateCommand(ed_doc, entPtr, session_before_snapshot, after_snapshot));
					property_edit_session_active = false;
				}

				if (selected) {
					// The outer "Properties" window is opened with zero WindowPadding (see
					// PushStyleVar below the ISelectionApi grab) so the reflected property
					// grid can butt up against the edges. That padding can't be reinstated
					// retroactively for this section, so fake it with indent + spacing.
					const float pad = ImGui::GetStyle().WindowPadding.x;
					ImGui::Dummy(ImVec2(0, pad));
					ImGui::Indent(pad);
					selected->on_inspector_imgui();
					ImGui::Unindent(pad);
					ImGui::Dummy(ImVec2(0, pad));

					if (!editor_ui || editor_ui_component != selected) {
						editor_ui = selected->create_editor_ui();
						editor_ui_component = selected;
					}
					if (editor_ui) {
						ImGui::Separator();
						editor_ui->draw();
					}
				}
				else {
					editor_ui.reset();
					editor_ui_component = nullptr;
				}
			}
		} else if (selected_vec.size() > 1) {
			draw_batch_mesh_properties(selected_vec);
		} else {
			ImGui::Text("Nothing selected\n");
		}
	}
	ImGui::End();
	ImGui::PopStyleVar(); // WindowPadding
}

void EdPropertyGrid::draw_batch_mesh_properties(const std::vector<EntityPtr>& selected) {
	std::vector<MeshComponent*> meshes;
	for (auto& eptr : selected) {
		Entity* e = eptr.get();
		if (!e) continue;
		Component* c = get_selected_component(e);
		if (c && c->is_a<MeshComponent>())
			meshes.push_back((MeshComponent*)c);
	}

	// "Properties" is opened with zero WindowPadding so the reflected property grid can butt
	// up against the edges (see PushStyleVar in draw()); reinstate padding for this section.
	const float pad = ImGui::GetStyle().WindowPadding.x;
	ImGui::Dummy(ImVec2(0, pad));
	ImGui::Indent(pad);

	ImGui::Text("%d entities selected (%d with a mesh component)", (int)selected.size(), (int)meshes.size());

	if (meshes.empty()) {
		ImGui::Text("No mesh components in selection\n");
		ImGui::Unindent(pad);
		return;
	}

	ImGui::Spacing();

	auto notify_changed = [&](MeshComponent* mc) {
		mc->editor_on_change_property();
		Entity* owner = mc->get_owner();
		if (owner)
			owner->editor_on_change_properties();
	};

	const ImGuiTableFlags table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("batch_mesh_props", 4, table_flags)) {
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 1.6f);
		ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthStretch, 0.8f);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableHeadersRow();

		auto draw_flag_row = [&](const char* label, std::function<bool(MeshComponent*)> get,
								  std::function<void(MeshComponent*, bool)> set) {
			int count_true = 0;
			for (auto mc : meshes)
				if (get(mc)) count_true++;

			ImGui::PushID(label);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::Text("%s", label);
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::Text("%d / %d", count_true, (int)meshes.size());
			ImGui::TableNextColumn();
			if (ImGui::Button("Set All", ImVec2(-FLT_MIN, 0))) {
				for (auto mc : meshes) { set(mc, true); notify_changed(mc); }
			}
			ImGui::TableNextColumn();
			if (ImGui::Button("Unset All", ImVec2(-FLT_MIN, 0))) {
				for (auto mc : meshes) { set(mc, false); notify_changed(mc); }
			}
			ImGui::PopID();
		};

		draw_flag_row("Nav Static",
			[](MeshComponent* mc) { return mc->get_nav_static(); },
			[](MeshComponent* mc, bool b) { mc->set_nav_static(b); });
		draw_flag_row("Add Collisions",
			[](MeshComponent* mc) { return mc->get_add_collision(); },
			[](MeshComponent* mc, bool b) { mc->set_add_collision(b); });
		draw_flag_row("Probe Bake",
			[](MeshComponent* mc) { return !mc->get_ignore_baking(); },
			[](MeshComponent* mc, bool b) { mc->set_ignore_baking(!b); });
		draw_flag_row("In Cubemap",
			[](MeshComponent* mc) { return !mc->get_ignore_cubemap(); },
			[](MeshComponent* mc, bool b) { mc->set_ignore_cubemap_view(!b); });
		draw_flag_row("Cast Shadows",
			[](MeshComponent* mc) { return mc->get_casts_shadows(); },
			[](MeshComponent* mc, bool b) { mc->set_casts_shadows(b); });

		ImGui::EndTable();
	}

	ImGui::Unindent(pad);
	ImGui::Dummy(ImVec2(0, pad));
}

void EdPropertyGrid::refresh_grid(const ISelectionApi& api) {
	
}

EdPropertyGrid::EdPropertyGrid(EditorDoc& ed_doc, const FnFactory<IPropertyEditor>& factory)
	: grid_cache(factory), ed_doc(ed_doc) {}

bool EdPropertyGrid::GridWithClasses::set_what_i_want_and_draw(std::vector<obj<BaseUpdater>> objs)
{
	auto any_nulls_in_cur = [&]() {
		for (auto o : cached_from_prev)
			if (o.get() == nullptr)
				return true;
		return false;
	};
	auto is_equal = [&]() {
		if (objs.size() != cached_from_prev.size())
			return false;
		for (int i = 0; i < objs.size(); i++) {
			if (objs[i].handle != cached_from_prev[i].handle)
				return false;
		}
		return true;
	};

	auto rebuild_grid = [&]() {
		grid.clear_all();

		for (auto o : cached_from_prev) {
			ASSERT(o.get());
			grid.add_class_to_grid(o.get());
			if (o->is_a<SpawnerComponent>()) {
				auto sc = (SpawnerComponent*)o.get();
				for (auto& [name, prop] : sc->obj.items()) {
					if (!prop.is_string())
						continue;
					if (name[0] == '_')
						continue;
					if (name == "model")
						grid.add_iproped_manual(new SpawnerModelProp(sc));
					else
						grid.add_iproped_manual(new SpawnerIProped(sc, name));
				}
			}
		}
	};

	if (!is_equal() || any_nulls_in_cur()) {
		cached_from_prev = objs;
		rebuild_grid();
	}

	grid.update();

	return grid.rows_had_changes;
}
