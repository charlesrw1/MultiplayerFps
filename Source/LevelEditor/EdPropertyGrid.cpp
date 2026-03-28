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

void EdPropertyGrid::draw_components(Entity* entity) {
	ASSERT(selected_component != 0);

	BaseUpdater* selectedC = eng->get_object(selected_component);
	ASSERT(selectedC);
	ASSERT(selectedC->is_a<Component>());
	ASSERT(((Component*)selectedC)->entity_owner == entity);

	auto draw_component = [&](Entity* e, Component* ec) {
		ASSERT(ec && e && ec->get_owner() == e);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		ImGui::PushID(ec);

		ImGuiSelectableFlags selectable_flags =
			ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
		if (ImGui::Selectable("##selectednode", ec->get_instance_id() == selected_component, selectable_flags,
							  ImVec2(0, 0))) {
			on_select_component(ec);
		}

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(5.f, 1.0));
		ImGui::SameLine();

		const char* s = ec->get_editor_outliner_icon();
		if (ec->get_type().get_is_lua_class())
			s = "eng/editor/script_lua.png";
		if (*s) {

			auto tex = Texture::load(s);
			if (tex) {
				tex->set_globally_referenced();
				my_imgui_image(tex, -1);
				ImGui::SameLine(0, 0);
			}
		}

		if (!ed_doc.is_this_object_not_inherited(ec))
			ImGui::TextColored(non_owner_source_color, ec->get_type().classname);
		else
			ImGui::Text(ec->get_type().classname);
		ImGui::PopID();
	};

	for (auto& c : entity->get_components())
		if (!c->dont_serialize_or_edit)
			draw_component(entity, c);
}

void EdPropertyGrid::draw() {

	auto& ss = ed_doc.selection_state;

	// this prevents use after free stuff
	if (ss->has_only_one_selected()) {
		auto selection = ss->get_only_one_selected();
		Entity* selected_as_ent = selection.get();
		const bool has_invalid_component = selected_component != 0 && !get_selected_component();
		if (!selected_as_ent || has_invalid_component) {
			sys_print(Warning, "EdPropertyGrid: ss->get_only_one_selected() returned null (rugpulled)\n");
			ss->clear_all_selected();
			refresh_grid();
		}
	}

	if (ImGui::Begin("Properties")) {
		if (ss->has_only_one_selected()) {

			auto selected = get_selected_component();
			if (selected && selected->is_a<SpawnerComponent>()) {
				auto sc = (SpawnerComponent*)selected;
				ImGui::Text("typename: %s\n", sc->get_spawner_type().c_str());
			}

			grid.update();

			if (grid.rows_had_changes) {

				auto e = ss->get_only_one_selected();
				e->editor_on_change_properties();
				e->post_change_transform_R();

				auto ec = get_selected_component();
				if (ec)
					ec->editor_on_change_property();

				on_property_change.invoke();
			}

		} else {
			ImGui::Text("Nothing selected\n");
		}
	}

	ImGui::End();

	if (ImGui::Begin("Components")) {

		if (!ss->has_any_selected()) {
			ImGui::Text("Nothing selected\n");
			selected_component = 0;
		} else if (!ss->has_only_one_selected()) {
			ImGui::Text("Select 1 entity to see components\n");
			selected_component = 0;
		} else {
			Entity* ent = ss->get_only_one_selected().get();
			ASSERT(ent);

			auto& comps = ent->get_components();
			{
				if (selected_component == 0 && comps.size() > 0)
					selected_component = comps[0]->get_instance_id();

				uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
										  ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
										  ImGuiTableFlags_Sortable;
				if (ImGui::BeginTable("animadfedBrowserlist", 1, ent_list_flags)) {

					ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, Color32{59, 0, 135}.to_uint());
					ImGuiSelectableFlags selectable_flags =
						ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;

					ImGui::SameLine();
					ImGui::Text(ent->get_type().classname);

					if (comps.size() > 0)
						draw_components(ent);

					ImGui::EndTable();

					if (ImGui::BeginDragDropTarget()) {
						const ImGuiPayload* payload =
							ImGui::AcceptDragDropPayload("AssetBrowserDragDrop", ImGuiDragDropFlags_AcceptPeekOnly);
						if (payload) {

							auto component_metadata =
								AssetRegistrySystem::get().find_for_classtype(&Component::StaticType);
							auto mesh_metadata = AssetRegistrySystem::get().find_for_classtype(&Model::StaticType);

							AssetOnDisk* resource = *(AssetOnDisk**)payload->Data;
							bool actually_accept = false;
							auto type = resource->type;
							if (type == component_metadata || type == mesh_metadata) {
								actually_accept = true;
							}

							if (actually_accept) {
								if ((payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop"))) {
									Entity* ent = ss->get_only_one_selected().get();
									ASSERT(ent);
									if (type == component_metadata) {
										auto comp_type = ClassBase::find_class(resource->filename.c_str());
										if (comp_type && comp_type->is_a(Component::StaticType)) {
											ed_doc.command_mgr->add_command(
												new CreateComponentCommand(ed_doc, ent, comp_type));
										}
									} else if (type == mesh_metadata) {
										ed_doc.command_mgr->add_command(new CreateMeshComponentCommand(
											ed_doc, ent, g_assets.find_sync<Model>(resource->filename).get()));
									}
								}
							}
						}
						ImGui::EndDragDropTarget();
					}
				}
			}
		}
	}
	ImGui::End();
}

void EdPropertyGrid::refresh_grid() {
	grid.clear_all();

	auto& ss = ed_doc.selection_state;

	if (!ss->has_any_selected())
		return;

	if (ss->has_only_one_selected() && ss->get_only_one_selected() /* can return null...*/) {
		auto entity = ss->get_only_one_selected();
		assert(entity);
		sys_print(Debug, "EdPropertyGrid::refresh_grid: adding to grid: %s\n", entity->get_type().classname);

		grid.add_class_to_grid(entity.get());

		auto& comps = entity->get_components();

		if (!comps.empty() && serialize_this_objects_children(entity.get())) {
			if (selected_component == 0)
				selected_component = comps[0]->get_instance_id();
			if (eng->get_object(selected_component) == nullptr ||
				eng->get_object(selected_component)->cast_to<Component>() == nullptr ||
				eng->get_object(selected_component)->cast_to<Component>()->get_owner() != entity.get())
				selected_component = comps[0]->get_instance_id();

			ASSERT(selected_component != 0);

			auto c = eng->get_object(selected_component)->cast_to<Component>();
			sys_print(Debug, "EdPropertyGrid::refresh_grid: adding to grid: %s\n", c->get_type().classname);

			ASSERT(c);
			grid.add_class_to_grid(c);

			if (c->is_a<SpawnerComponent>()) {
				auto sc = (SpawnerComponent*)c;
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
	}
}

EdPropertyGrid::EdPropertyGrid(EditorDoc& ed_doc, const FnFactory<IPropertyEditor>& factory)
	: ed_doc(ed_doc), factory(factory), grid(factory) {
	auto& ss = ed_doc.selection_state;
	ss->on_selection_changed.add(this, &EdPropertyGrid::refresh_grid);
	ed_doc.post_node_changes.add(this, &EdPropertyGrid::refresh_grid);
	ed_doc.on_close.add(this, &EdPropertyGrid::on_close);
	ed_doc.on_component_deleted.add(this, &EdPropertyGrid::on_ec_deleted);
	ed_doc.on_component_created.add(this, &EdPropertyGrid::on_select_component);
}