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


				const bool changed = grid_cache.set_what_i_want_and_draw(what_i_want);

				if (changed) {
					Entity* e = selected_vec.at(0).get();
					ASSERT(e);
					e->editor_on_change_properties();
					e->post_change_transform_R();

					auto ec = get_selected_component(selected_vec.at(0).get());
					if (ec)
						ec->editor_on_change_property();

					on_property_change_internal.invoke();
				}

				if (selected) {
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
		} else {
			ImGui::Text("Nothing selected\n");
		}
	}
	ImGui::End();
	ImGui::PopStyleVar(); // WindowPadding
}

void EdPropertyGrid::refresh_grid(const ISelectionApi& api) {
	
}

EdPropertyGrid::EdPropertyGrid(const FnFactory<IPropertyEditor>& factory) : grid_cache(factory) {}

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
