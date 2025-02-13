#ifdef EDITOR_BUILD
#include "AnimAssetEditorLocal.h"
#include <stdexcept>
#include <SDL2/SDL.h>

#include "Someutils.h"
#include "Framework/MyImguiLib.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Animation/AnimationUtil.h"

#include "GameEnginePublic.h"
#include "OsInput.h"

#include "Render/RenderObj.h"

#include "Framework/ObjectSerialization.h"

#include "Framework/DictWriter.h"
#include <fstream>

#include "AnimAssetEditorLocal.h"

#include "Animation/Event.h"

static AnimationEditorTool g_animseq_editor_static;
IEditorTool* g_animseq_editor = &g_animseq_editor_static;

void AnimationEditorTool::add_to_obj(Render_Object& obj, float dt)
{
	auto mod = g_animseq_editor_static.outputModel;
	if (!mod || !mod->get_skel())
		return;

	Pose pose;
	util_calc_rotations(mod->get_skel(), g_animseq_editor_static.sequence->seq, g_animseq_editor_static.animEdit->CURRENT_TIME, nullptr, pose);
	auto& animator = g_animseq_editor_static.animator;
	animator.model = mod;
	animator.cached_bonemats.resize(mod->get_skel()->get_num_bones());
	//animator.matrix_palette.resize(mod->get_skel()->get_num_bones());
	util_localspace_to_meshspace_ptr_2(pose, animator.cached_bonemats.data(), mod->get_skel());

	animator.ConcatWithInvPose();

	//obj.animator = &animator;
	obj.model = mod;
	obj.visible = true;

	obj.transform = mod->get_root_transform();
}

void AnimationEditorTool::tick(float dt)
{
	EditorTool3d::tick(dt);

	auto mod = g_animseq_editor_static.outputModel;
	if (outputObj.is_valid()&&mod&&mod->get_skel()) {
		Render_Object o;
		add_to_obj(o, dt);
		idraw->get_scene()->update_obj(outputObj, o);

		//glm::vec3 hipsCenter = animator.get_global_bonemats()[0][3];
		//hipsCenter = outputModel->get_root_transform() * glm::vec4(hipsCenter, 1.0);
		//view = View_Setup(camera.position + hipsCenter, camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);
	}
}

void AnimationEditorTool::imgui_draw()
{
	if (ImGui::Begin("Main properties")) {

		if (!outputModel) {
			ImGui::TextColored(color32_to_imvec4({ 150,150,150 }), "No compilied model");
		}
		propGrid.update();
	}
	ImGui::End();

	animEdit->draw_imgui();

	IEditorTool::imgui_draw();
}


EditModelAnimations::EditModelAnimations()
{
	g_animseq_editor_static.on_close.add(this, &EditModelAnimations::on_quit);
	g_animseq_editor_static.on_pre_save.add(this, &EditModelAnimations::on_presave);
	g_animseq_editor_static.on_post_save.add(this, &EditModelAnimations::on_postsave);

	g_animseq_editor_static.on_start.add(this, &EditModelAnimations::on_start);
}


#include "Framework/Curve.h"
void EditModelAnimations::on_postsave()
{
	on_start();
}
void EditModelAnimations::on_presave()
{
	auto& c = *g_animseq_editor_static.animImportSettings;

	auto& e = curveedit.get_event_array();
	for (int i = 0; i < e.size(); i++) {
		auto a = (EventSequenceItem*)e.at(i).get();
		a->event->frame = a->time_start;
		a->event->frame_duration = a->time_end - a->time_start;
		a->event->editor_layer = a->y_coord;
		c.events.push_back(a->event.release());
	}

	auto& cur = curveedit.get_curve_array();
	c.curves.clear();
	//BakedCurve bc;
	//bc.bake_from(cur, curveedit.max_x_value, 0.0, 1.0);
	for (int i = 0; i < cur.size(); i++) {
		c.curves.push_back(cur[i]);
	}

	selected_event_item = nullptr;
	curveedit.clear_all();
	eventdetails.clear_all();
}

void EditModelAnimations::on_quit()
{
	selected_event_item = nullptr;
	curveedit.clear_all();
	eventdetails.clear_all();
}

static void context_menu_callback(CurveEditorImgui* ptr) {


	auto classes = ClassBase::get_subclasses<AnimationEvent>();
	for (; !classes.is_end(); classes.next()) {
		if (!classes.get_type()->allocate) continue;
		bool selected = false;
		if (ImGui::Selectable(classes.get_type()->classname, &selected)) {
			auto ev = (AnimationEvent*)classes.get_type()->allocate();

			ptr->add_item_from_menu(std::unique_ptr<EventSequenceItem>(new EventSequenceItem(ev)));
			ImGui::CloseCurrentPopup();
		}
	}
}


void EditModelAnimations::on_start()
{
	CURRENT_TIME = 0.0;
	curveedit.clear_all();
	selected_event_item = nullptr;
	curveedit.callback = context_menu_callback;

	auto seq = g_animseq_editor_static.sequence;

	curveedit.max_x_value = seq->seq->num_frames;

	auto& c = *g_animseq_editor_static.animImportSettings;
	for (int i = 0; i < c.events.size(); i++) {
		auto ev = std::move(c.events[i]);
		auto ptr = ev;
		EventSequenceItem* esi = new EventSequenceItem(ptr/* release ownership*/);
		esi->time_start = esi->event->frame;
		esi->time_end = esi->event->frame_duration;
		esi->y_coord = esi->event->editor_layer;
		curveedit.add_item_from_menu(std::unique_ptr<EventSequenceItem>(esi));
	}
	curveedit.clear_all();
	auto& cur = c.curves;
	for (auto& c : cur) {
		curveedit.add_curve(c);
	}
	c.events.clear();
}
void EditModelAnimations::draw_imgui()
{
	
	EventSequenceItem* selected = (EventSequenceItem*)curveedit.get_selected_event();
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
	CURRENT_TIME = curveedit.current_time / 30.0;
	//seqimgui.current_time = curveedit.current_time;
}





#include "Assets/AssetDatabase.h"
#include "Game/StdEntityTypes.h"
extern ConfigVar ed_default_sky_material;
void AnimationEditorTool::post_map_load_callback()
{
	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini AnimSeqEditor.ini");


	assert(!importSettings);

	assert(!outputModel);
	assert(!animImportSettings);

	auto str = std::string(get_doc_name());
	auto slash = str.rfind('/');
	std::string modelName = str.substr(0, slash);
	std::string animName = str.substr(slash + 1);

	// try to find def_name
	std::string def_name = modelName + ".mis";

	auto file = FileSys::open_read_game(def_name.c_str());

	if (!file) {
		sys_print(Error, "AnimEditor: couldnt find path %s\n", def_name.c_str());
	}
	else {
		DictParser dp;
		dp.load_from_file(file.get());
		StringView tok;
		dp.read_string(tok);

		importSettings = read_object_properties<ModelImportSettings>(nullptr, dp, tok);
	}
	if (!importSettings) {
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "close_ed");
		return;
	}


	for (int i = 0; i < importSettings->animations.size(); i++) {
		if (importSettings->animations[i].clipName == animName)
			animImportSettings = &importSettings->animations[i];	// kinda unsafe, but array shouldnt be modified
	}
	if (!animImportSettings) {
		importSettings->animations.push_back({});
		animImportSettings = &importSettings->animations.back();
		animImportSettings->clipName = animName;
	}
	assert(importSettings);
	assert(animImportSettings);


	auto ti = &animImportSettings->get_type();
	while (ti) {
		if (ti->props)
			propGrid.add_property_list_to_grid(ti->props, animImportSettings);
		ti = ti->super_typeinfo;
	}


	outputObj = idraw->get_scene()->register_obj();
	///outputEntity->Mesh->



	outputModel = default_asset_load<Model>((modelName + ".cmdl").c_str());	// find the compilied model, this could be an error and loading still 'works'
	if (!outputModel)
		sys_print(Debug,"compilied model didnt load but loading .def didnt error, continuing as normal\n");
	else {
		int remapIndx;
		sequence = g_assets.find_sync<AnimationSeqAsset>(modelName + "/" + animName).get();// outputModel->get_skel()->find_clip(animName, remapIndx);
	}

	on_start.invoke();
}

void AnimationEditorTool::close_internal()
{
	EditorTool3d::close_internal();

	on_close.invoke();

	idraw->get_scene()->remove_obj(outputObj);

	outputModel = nullptr;
	delete importSettings;
	importSettings = nullptr;
	animImportSettings = nullptr;

	propGrid.clear_all();
}
#include "Compiliers.h"
bool AnimationEditorTool::save_document_internal()
{
	ASSERT(importSettings);

	on_pre_save.invoke();

	DictWriter write;
	write_object_properties(importSettings, nullptr, write);


	auto str = std::string(get_doc_name());
	auto slash = str.rfind('/');
	std::string modelName = str.substr(0, slash);
	std::string animName = str.substr(slash + 1);

	{
		auto outfile = FileSys::open_write_game(modelName + ".mis");
		outfile->write(write.get_output().data(), write.get_output().size());
	}

	if (!outputModel) {
		auto fullpath = FileSys::get_game_path() + ("/" + modelName + ".mis");
		ModelCompilier::compile_from_settings(fullpath.c_str(), importSettings);
		outputModel = default_asset_load<Model>(get_doc_name());
		int dummy;

		on_post_save.invoke();
	}
	else {

		g_assets.reload_async(outputModel, [](GenericAssetPtr ptr) {

			if (!g_animseq_editor_static.outputModel)
				return;

			auto str = std::string(g_animseq_editor_static.get_doc_name());
			auto slash = str.rfind('/');
			auto animName = str.substr(slash + 1);

			int dummy;
			g_animseq_editor_static.on_post_save.invoke();
		});

	//	g_assets.reload_sync(outputModel);
	}
	return true;
}



#endif