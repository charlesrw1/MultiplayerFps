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

void EdPropertyGrid::draw_components(const ISelectionApi& api, Entity* entity) {
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

		// TODO: Check if object is inherited for visual distinction
		// if (!ed_doc.is_this_object_not_inherited(ec))
		//	ImGui::TextColored(non_owner_source_color, ec->get_type().classname);
		// else
		ImGui::Text(ec->get_type().classname);
		ImGui::PopID();
	};

	for (auto& c : entity->get_components())
		if (!c->dont_serialize_or_edit)
			draw_component(entity, c);
}

void EdPropertyGrid::draw(const ISelectionApi& api) {
	last_api = &api;
	auto selected_vec = api.get_selected();

	// this prevents use after free stuff
	if (selected_vec.size() == 1) {
		EntityPtr selection = selected_vec.at(0);
		Entity* selected_as_ent = selection.get();
		const bool has_invalid_component = selected_component != 0 && !get_selected_component();
		if (!selected_as_ent || has_invalid_component) {
			sys_print(Warning, "EdPropertyGrid: ss->get_only_one_selected() returned null (rugpulled)\n");
			// ss->clear_all_selected();
			refresh_grid(api);
		}
	}

	if (ImGui::Begin("Properties")) {
		if (selected_vec.size() == 1) {
			auto selected = get_selected_component();
			if (selected && selected->is_a<SpawnerComponent>()) {
				auto sc = (SpawnerComponent*)selected;
				ImGui::Text("typename: %s\n", sc->get_spawner_type().c_str());
			}

			grid.update();

			if (grid.rows_had_changes) {
				Entity* e = selected_vec.at(0).get();
				ASSERT(e);
				e->editor_on_change_properties();
				e->post_change_transform_R();

				auto ec = get_selected_component();
				if (ec)
					ec->editor_on_change_property();

				on_property_change_internal.invoke();
			}
		} else {
			ImGui::Text("Nothing selected\n");
		}
	}

	ImGui::End();

	if (ImGui::Begin("Components")) {

		if (selected_vec.empty()) {
			ImGui::Text("Nothing selected\n");
			selected_component = 0;
		} else if (selected_vec.size() > 1) {
			ImGui::Text("Select 1 entity to see components\n");
			selected_component = 0;
		} else {
			Entity* ent = selected_vec.at(0).get();
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
						draw_components(api, ent);

					ImGui::EndTable();
				}
			}
		}
	}
	ImGui::End();
}

void EdPropertyGrid::refresh_grid(const ISelectionApi& api) {
	grid.clear_all();

	auto selected = api.get_selected();
	if (!selected.empty())
		return;

	if (selected.size() == 1) {
		auto* entity = selected.at(0).get();
		assert(entity);
		sys_print(Debug, "EdPropertyGrid::refresh_grid: adding to grid: %s\n", entity->get_type().classname);

		grid.add_class_to_grid(entity);

		auto& comps = entity->get_components();

		if (!comps.empty() && serialize_this_objects_children(entity)) {
			if (selected_component == 0)
				selected_component = comps[0]->get_instance_id();
			if (eng->get_object(selected_component) == nullptr ||
				eng->get_object(selected_component)->cast_to<Component>() == nullptr ||
				eng->get_object(selected_component)->cast_to<Component>()->get_owner() != entity)
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

EdPropertyGrid::EdPropertyGrid(const FnFactory<IPropertyEditor>& factory) : factory(factory), grid(factory) {}