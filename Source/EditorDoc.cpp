#include "EditorDoc.h"
#include "imgui.h"
#include "glad/glad.h"
#include "Game_Engine.h"
#include <algorithm>


void AssetBrowser::init()
{
	edmodels.clear();
	char buffer[256];
	while (Files::iterate_files_in_dir("./Data/Models/*", buffer, 256)) {
		EdModel em;
		em.name = buffer; // load the models once you need them
		em.m = nullptr;
		edmodels.push_back(em);
	}
	update_remap();

	asset_name_filter[0] = 0;
}
void AssetBrowser::handle_input(const SDL_Event& inp)
{
	if (inp.type == SDL_MOUSEWHEEL)
		increment_index(inp.wheel.y);
}
void AssetBrowser::open(bool keyfocus)
{
	set_keyboard_focus = keyfocus;
}
void AssetBrowser::close() {}

void EditorDoc::open_doc(const char* levelname)
{
	leveldoc = eng->level;

	for (int i = 0; i < leveldoc->espawns.size(); i++) {
		Level::Entity_Spawn* espawn = &leveldoc->espawns[i];
		EditorNode* node = new EditorNode;
		node->index = i;
		node->position = espawn->position;
		node->rotation = espawn->rotation;
		node->scale = espawn->scale;

		const char* model = espawn->spawnargs.get_string("model", "");
		if (*model) {
			node->model = FindOrLoadModel(model);
		}

		nodes.push_back(node);
	}
	assets.init();
}

void EditorDoc::save_doc()
{
}

void EditorDoc::close_doc()
{
}


bool EditorDoc::handle_event(const SDL_Event& event)
{
	if (assets_open)
		assets.handle_input(event);

	switch (event.type)
	{
	case SDL_MOUSEBUTTONDOWN:
		//SDL_SetRelativeMouseMode(SDL_TRUE);

		break;
	case SDL_MOUSEBUTTONUP:
		//SDL_SetRelativeMouseMode(SDL_FALSE);

		break;
	case SDL_KEYDOWN:
		switch (event.key.keysym.scancode)
		{
		case SDL_SCANCODE_M:
			if (assets_open)
				assets.close();
			else
				assets.open(true);
			assets_open = !assets_open;
			break;
		}


		break;
	}

	return true;
}

void EditorDoc::update()
{
	if (eng->game_focused) {
		int x, y;
		SDL_GetRelativeMouseState(&x, &y);
		camera.update_from_input(eng->keys, x, y);
	}


	// build physics world

	eng->phys.ClearObjs();
	{
		PhysicsObject obj;
		obj.is_level = true;
		obj.solid = true;
		obj.is_mesh = true;
		obj.mesh.structure = &eng->level->collision.bvh;
		obj.mesh.verticies = &eng->level->collision.verticies;
		obj.mesh.tris = &eng->level->collision.tris;

		eng->phys.AddObj(obj);
	}
	eng->phys.AddObj()

}

void EditorDoc::scene_draw_callback()
{
}

void EditorDoc::overlays_draw()
{
}

void EditorDoc::imgui_draw()
{
	if (assets_open) assets.imgui_draw();

    static int item_selected = 0;

	ImGui::SetNextWindowPos(ImVec2(10, 10));

	float alpha = (eng->game_focused) ? 0.3 : 0.7;

	ImGui::SetNextWindowBgAlpha(alpha);
	//ImGui::SetNextWindowSize(ImVec2(250, 500));
	uint32_t flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;
	if (ImGui::Begin("EdDoc",nullptr, flags))
	{
		uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("Entity list",1, ent_list_flags, ImVec2(0, 300.f), 0.f))
        {
            // Declare columns
            // We use the "user_id" parameter of TableSetupColumn() to specify a user id that will be stored in the sort specifications.
            // This is so our sort function can identify a column given our own identifier. We could also identify them based on their index!
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableHeadersRow();


            for (int row_n = 0; row_n < nodes.size(); row_n++)
            {
				EditorNode* n = nodes[row_n];
				Level::Entity_Spawn* es = &leveldoc->espawns[n->index];

				const char* name = es->name.c_str();
                //if (!filter.PassFilter(item->Name))
                //    continue;

                const bool item_is_selected = item_selected == row_n;
                ImGui::PushID(row_n);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.f);

                // For the demo purpose we can select among different type of items submitted in the first column
                ImGui::TableSetColumnIndex(0);
                char label[32];
                //sprintf(label, "%04d", item->ID);
                {
                    ImGuiSelectableFlags selectable_flags =ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                    if (ImGui::Selectable(name, item_is_selected, selectable_flags, ImVec2(0, 0.f)))
                    {
						item_selected = row_n;
						selected_node = n;
                    }
                }

				ImGui::PopID();
            }
        }
        ImGui::EndTable();

		if (selected_node != nullptr) {

			ImGui::SeparatorText("Property editor");

			uint32_t prop_editor_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

			if (ImGui::BeginTable("Property editor", 2, prop_editor_flags))
			{
				ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed,80.f, 0);
				ImGui::TableSetupColumn("Value", 0, 0.0f, 1);
				ImGui::TableHeadersRow();

				ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

				Dict* d = &leveldoc->espawns[selected_node->index].spawnargs;

				int cell = 0;
				for (auto& kv : d->keyvalues)
				{
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::PushID(cell);
					ImGui::TextUnformatted(kv.first.c_str());

					ImGui::TableSetColumnIndex(1);
					static char buffer[256];
					memcpy(buffer, kv.second.c_str(), kv.second.size());
					buffer[kv.second.size()] = 0;
					if (ImGui::InputText("##cell", buffer, 256)) {
						kv.second = buffer;
					}

					ImGui::PopID();

					cell++;
				}
				ImGui::PopStyleColor();
			}
			ImGui::EndTable();



			ImGui::DragFloat3("Position", &selected_node->position.x);
			ImGui::DragFloat3("Rotation", &selected_node->rotation.x);
			ImGui::DragFloat3("Scale", &selected_node->scale.x);
			


		}
	}
	ImGui::End();
}

Model* AssetBrowser::get_model()
{
	if (selected_real_index == -1) return nullptr;
	EdModel* em = &edmodels[selected_real_index];
	if (em->havent_loaded) {
		em->m = FindOrLoadModel(em->name.c_str());
		em->havent_loaded = false;
	}
	return em->m;
}

void AssetBrowser::increment_index(int amt)
{
	if (remap.size() == 0) {
		return;
	}

	int remapped_index = -1;
	for (int i = 0; i < remap.size(); i++) {
		if (remap[i] == selected_real_index) {
			remapped_index = i;
			break;
		}
	}
	if (remapped_index == -1) {
		selected_real_index = remap[0];
	}
	else {
		remapped_index -= amt;
		remapped_index %= remap.size();
		if (remapped_index < 0) remapped_index += remap.size();
		selected_real_index = remap[remapped_index];
	}
}

void AssetBrowser::update_remap()
{
	remap.clear();
	for (int i = 0; i < edmodels.size(); i++) {
		if (filter_match_case) {
			if (edmodels[i].name.find(asset_name_filter) == std::string::npos) continue;
		}
		else {
			static std::string s;
			static std::string s2;
			s = edmodels[i].name;
			for (int i = 0; i < s.size(); i++)s[i] = toupper(s[i]);
			s2 = asset_name_filter;
			for (int j = 0; j < s2.size(); j++)s2[j] = toupper(s2[j]);
			if (s.find(s2) == std::string::npos) continue;
		}
		remap.push_back(i);
	}


	std::sort(remap.begin(), remap.end(),
		[&](int a, int b) {
			return edmodels[a].name < edmodels[b].name;
		});
}

void AssetBrowser::imgui_draw()
{
	if (ImGui::Begin("EdModels")) {

		uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;

		if (set_keyboard_focus) {
			ImGui::SetKeyboardFocusHere();
			set_keyboard_focus = false;
		}
		static bool match_case = false;
		if (ImGui::InputTextWithHint("Find", "search for model", asset_name_filter, 256)) {
			update_remap();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Match Case", &match_case);


		if (ImGui::BeginTable("Model list", 1, ent_list_flags, ImVec2(0, 300.f), 0.f))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
			ImGui::TableSetupScrollFreeze(1, 1);
			ImGui::TableHeadersRow();

			for (int row_n = 0; row_n < remap.size(); row_n++)
			{
				int this_real_index = remap[row_n];

				const char* name = edmodels[this_real_index].name.c_str();
				bool item_is_selected = this_real_index == selected_real_index;
				ImGui::PushID(row_n);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.f);
				ImGui::TableSetColumnIndex(0);
				{
					ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
					if (ImGui::Selectable(name, item_is_selected, selectable_flags, ImVec2(0, 0.f))) {
						selected_real_index = this_real_index;
					}
				}

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

const View_Setup& EditorDoc::get_vs()
{
	View_Setup setup;
	setup.fov = glm::radians(70.f);
	setup.near = 0.01;
	setup.far = 100.0;
	setup.height = eng->window_h.integer();
	setup.width = eng->window_w.integer();
	setup.origin = camera.position;
	setup.front = camera.front;
	setup.view = camera.get_view_matrix();
	setup.proj = glm::perspective(setup.fov, (float)setup.width / setup.height, setup.near, setup.far);
	setup.viewproj = setup.proj * setup.view;

	return setup;
}
