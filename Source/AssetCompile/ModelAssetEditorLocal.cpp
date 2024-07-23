#include "AssetCompile/ModelAssetEditorLocal.h"
#include <stdexcept>
#include <SDL2/SDL.h>

#include "Someutils.h"
#include "Framework/MyImguiLib.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Animation/AnimationUtil.h"

#include "GameEnginePublic.h"
#include "OsInput.h"

static ModelEditorTool g_model_editor_static;
IEditorTool* g_model_editor = &g_model_editor_static;

void ModelEditorTool::ui_paint()
{
}

bool ModelEditorTool::handle_event(const SDL_Event& event)
{
	return false;
}

void ModelEditorTool::tick(float dt)
{
	auto window_sz = eng->get_game_viewport_size();
	float aratio = (float)window_sz.y / window_sz.x;
	{
		int x = 0, y = 0;
		if (eng->is_game_focused()) {
			SDL_GetRelativeMouseState(&x, &y);
				camera.update_from_input(eng->get_input_state()->keys, x, y, glm::mat4(1.f));
		}
	}



	view = View_Setup(camera.position, camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);

	if (get_focus_state() != editor_focus_state::Background && compilied_model) {
		if (!object.is_valid())
			object = idraw->register_obj();
		Render_Object obj;
		obj.model = compilied_model;
		obj.transform = glm::mat4(1.f);
		obj.visible = true;
		editanims.add_to_obj(obj, dt);
		idraw->update_obj(object,obj);
	}
}

void ModelEditorTool::imgui_draw()
{
	if (ImGui::Begin("Main properties")) {

		if (!compilied_model) {
			ImGui::TextColored(color32_to_imvec4({ 150,150,150 }), "No compilied model");
		}
		const char* strs[] = { "General","Animation","Physics" };
		if (ImGui::BeginCombo("Edit mode", strs[(int)edit_state_type])) {
			for (int i = 0; i < 3; i++) {
				if (ImGui::Selectable(strs[i], (int)edit_state_type == i)) {
					edit_state_type = ModelEditType(i);
				}
			}
			ImGui::EndCombo();
		}
	}
	ImGui::End();

		editanims.draw_imgui();
	//switch (edit_state_type) {
	//case ModelEditType::Animation:
	//	break;
	//}

	IEditorTool::imgui_draw();
}
void EditModelAnimations::add_to_obj(Render_Object& obj, float dt)
{
	auto mod = g_model_editor_static.compilied_model;
	if (!mod || !mod->get_skel())
		return;

	if (selected_index != -1) {
		auto& a = names[selected_index];
		Pose pose;
		util_calc_rotations(mod->get_skel(), a.seq, CURRENT_TIME, nullptr, pose);
		animator.model = mod;
		animator.cached_bonemats.resize(mod->get_skel()->get_num_bones());
		animator.matrix_palette.resize(mod->get_skel()->get_num_bones());
		util_localspace_to_meshspace_ptr_2(pose, animator.cached_bonemats.data(), mod->get_skel());

		animator.ConcatWithInvPose();

		obj.animator = &animator;

		obj.transform = mod->get_root_transform();
	}
}
AnimationClip_Load& EditModelAnimations::find_or_create_for_selected()
{
	return g_model_editor_static.model_def->str_to_clip_def[names[selected_index].name];
}
 void EditModelAnimations::init_from_model(const Model* m) {
	animator = AnimatorInstance();
	names.clear();
	CURRENT_TIME = 0.0;
	selected_index = -1;
	curveedit.clear_all();
	selected_event_item = nullptr;
	if (!m)
		return;
	if (!m->get_skel())
		return;
	for (auto& clip : m->get_skel()->get_clips_hashmap()) {
		if (clip.second.remap_idx != -1) continue;
		names.push_back({ clip.first,clip.second.ptr });
	}
}
#include "Framework/Curve.h"
void EditModelAnimations::on_select_new_animation(int next)
{
	if (next == selected_index)
		return;
	if (selected_index != -1) {
		// save off stuff to model def

		auto& s = names[selected_index];
		auto& c = find_or_create_for_selected();

		auto& e = seqimgui.get_item_array();
		for (int i = 0; i < e.size(); i++) {
			auto a = (EventSequenceItem*)e.at(i).get();
			a->event->frame = a->time_start;
			a->event->frame_duration = a->time_end-a->time_start;
			a->event->editor_layer = a->track_index;
			c.events.push_back(std::move(a->event));
		}

		auto& cur = curveedit.get_curve_array();
		c.curves.clear();
			BakedCurve bc;
			bc.bake_from(cur, curveedit.max_x_value, 0.0, 1.0);
		for (int i = 0; i < cur.size(); i++) {
			c.curves.push_back(cur[i]);
		}

		selected_event_item = nullptr;
		curveedit.clear_all();
		eventdetails.clear_all();
		seqimgui.clear_all();
	}
	selected_index = next;
	if (selected_index != -1) {
		auto& a = names[selected_index];
		curveedit.max_x_value = a.seq->num_frames;
		seqimgui.max_x_value = a.seq->num_frames;
		auto& c = find_or_create_for_selected();
		for (int i = 0; i < c.events.size(); i++) {
			auto ev = std::move(c.events[i]);
			auto ptr = ev.release();
			EventSequenceItem* esi = new EventSequenceItem(ptr/* release ownership*/);
			esi->time_start = esi->event->frame;
			esi->time_end = esi->event->frame_duration;
			esi->track_index = esi->event->editor_layer;

			seqimgui.add_item_direct(esi);
		}
		curveedit.clear_all();
		auto& cur = c.curves;
		for (auto& c : cur) {
			curveedit.add_curve(c);
		}
		c.events.clear();
	}
	CURRENT_TIME = 0.0;
}
void EditModelAnimations::draw_imgui()
{
	// draw list of all animations
	// draw selected animation info data
	// draw curves+events
	// draw event details
	if (ImGui::Begin("Animatdfaion List")) {

		uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;

		static bool match_case = false;
		ImGui::SetNextItemWidth(200.0);
		ImGui::InputTextWithHint("FILTER", "filter animation name", name_filter, 256);
		const int name_filter_len = strlen(name_filter);

		std::string all_lower_cast_filter_name;
		if (!match_case) {
			all_lower_cast_filter_name = name_filter;
			for (int i = 0; i < name_filter_len; i++)
				all_lower_cast_filter_name[i] = tolower(all_lower_cast_filter_name[i]);
		}

		if (ImGui::BeginTable("animadfedBrowserlist", 1, ent_list_flags))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);

			ImGui::TableHeadersRow();

			for (int row_n = 0; row_n < names.size(); row_n++)
			{
				auto& res = names[row_n];

				if (name_filter_len > 0) {
					if (res.name.find(name_filter) == std::string::npos)
						continue;
				}

				ImGui::PushID(res.name.c_str());
				const bool item_is_selected = row_n == selected_index;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
				if (ImGui::Selectable("##selectednode", item_is_selected, selectable_flags, ImVec2(0, 0))) {
					on_select_new_animation(row_n);
				}

				ImGui::SameLine();
				ImGui::Text(res.name.c_str());

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();

	seqimgui.draw();
	EventSequenceItem* selected = seqimgui.get_selected_index() == -1 ? nullptr : (EventSequenceItem*)seqimgui.get_item_array()[seqimgui.get_selected_index()].get();
	if (selected != selected_event_item) {
		eventdetails.clear_all();
		selected_event_item = selected;
		if (selected_event_item) {
			// add properties, be careful about dangling pointers here
			const ClassTypeInfo* ti = &selected_event_item->event->get_type();
			while (ti) {
				if (ti->props)
					eventdetails.add_property_list_to_grid(ti->props, selected_event_item->event.get());
				ti = ti->super_typeinfo;
			}
		}
	}
	if (ImGui::Begin("Anim Event Details")) {
		eventdetails.update();
	}
	ImGui::End();

	curveedit.draw();
	CURRENT_TIME = curveedit.current_time/30.0;
	seqimgui.current_time = curveedit.current_time;

}
void ModelEditorTool::draw_menu_bar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New")) {
				open("");
			}
			if (ImGui::MenuItem("Open", "Ctrl+O")) {
				open_the_open_popup();

			}
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				save();
			}

			ImGui::EndMenu();
		}
		
		ImGui::EndMenuBar();
	}
}
const View_Setup& ModelEditorTool::get_vs()
{
	// TODO: insert return statement here
	return view;
}

void ModelEditorTool::overlay_draw()
{
}

void ModelEditorTool::on_change_focus(editor_focus_state newstate)
{
	if (newstate == editor_focus_state::Closed)
		close();
	else if (newstate == editor_focus_state::Background)
		idraw->remove_obj(object);
}

void ModelEditorTool::init()
{
}

bool ModelEditorTool::can_save_document()
{
	return true;
}

const char* ModelEditorTool::get_editor_name()
{
	return "Model Editor";
}

bool ModelEditorTool::has_document_open() const
{
	return is_open;
}

void ModelEditorTool::open_document_internal(const char* name)
{
	if (strlen(name) > 0) {
		// try to find def_name
		std::string def_name = strip_extension(name) + ".def";
		compilied_model = mods.find_or_load(name);	// find the compilied model, this could be an error and loading still 'works'
		std::string fullpath = "./Data/Models/" + def_name;
		auto file = FileSys::open_read_os(fullpath.c_str());
		bool error = false;
		if (!file) {
			error = true;
			sys_print("!!! ModelEditor: couldnt find path %s\n", fullpath.c_str());
		}
		else {
			DictParser parser;
			parser.load_from_file(file.get());
			model_def = std::make_unique<ModelDefData>();
			try {
				model_def->read_from_dict(parser);		
			}
			catch (std::runtime_error er) {
				sys_print("!!! ModelEditor caught error in loading .def file: %s", er.what());
				error = true;
			}
		}
		if (error) {
			set_empty_name();
			compilied_model = nullptr;
			model_def.reset(new ModelDefData());	// allocate new model def data
		}
		else {
			if (!compilied_model)
				sys_print("*** compilied model didnt load but loading .def didnt error, continuing as normal\n");

			set_doc_name(name);
		}
	}
	else {
		set_empty_name();
		compilied_model = nullptr;
		model_def.reset(new ModelDefData());
	}

	edit_state_type = ModelEditType::General;

	editanims.init_from_model(compilied_model);

	is_open = true;
}

void ModelEditorTool::close_internal()
{
	idraw->remove_obj(object);
	model_def.reset();
	is_open = false;
}
#include "Framework/DictWriter.h"
#include <fstream>
bool ModelEditorTool::save_document_internal()
{
	ASSERT(model_def);

	DictWriter write;
	model_def->write_to_dict(write);
	auto out = write.get_output();
	std::ofstream outfile("abc.txt");
	outfile.write(out.c_str(), out.size());

	DictParser in;
	in.load_from_memory((uint8_t*)out.data(), out.size(), "a");
	model_def->read_from_dict(in);
	return false;
}

void EventTimelineSequencer::context_menu_callback() {
	auto classes = ClassBase::get_subclasses<AnimationEvent>();
	for (; !classes.is_end(); classes.next()) {
		if (!classes.get_type()->allocate) continue;
		bool selected = false;
		if (ImGui::Selectable(classes.get_type()->classname, &selected)) {
			auto ev = (AnimationEvent*)classes.get_type()->allocate();
			add_item_from_menu(new EventSequenceItem(ev));
			ImGui::CloseCurrentPopup();
		}
	}
}