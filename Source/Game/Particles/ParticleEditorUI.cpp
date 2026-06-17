#ifdef EDITOR_BUILD
#include "ParticleEditorUI.h"
#include "ParticleModules.h"
#include "ParticleAsset.h"
#include "Game/Components/ParticleSystemComponent.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetBrowser.h"
#include "AssetTools/AssetTemplates.h"
#include "imgui.h"

ParticleSystemEditorUi::ParticleSystemEditorUi(ParticleSystemComponent* comp)
	: comp(comp)
{
	curve_editor_popup.max_x_value = 1.0;
	curve_editor_popup.min_y_value = 0.0;
	curve_editor_popup.max_y_value = 1.0;
	curve_editor_popup.show_add_curve_button = false;
}

bool ParticleSystemEditorUi::draw_module_header(const char* name, bool& enabled, bool& expanded)
{
	ImGui::PushID(name);
	ImGui::Checkbox("##enabled", &enabled);
	ImGui::SameLine();
	expanded = ImGui::CollapsingHeader(name, expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0);
	ImGui::PopID();
	return expanded && enabled;
}

void ParticleSystemEditorUi::draw_minmax_curve(const char* label, MinMaxCurve& curve)
{
	ImGui::PushID(label);
	ImGui::Text("%s", label);
	ImGui::SameLine(120);

	const char* mode_names[] = {"Constant", "Random Between Constants", "Curve", "Random Between Curves"};
	int mode_int = (int)curve.mode;
	ImGui::SetNextItemWidth(180);
	if (ImGui::Combo("##mode", &mode_int, mode_names, 4))
		curve.mode = (MinMaxCurveMode)mode_int;

	switch (curve.mode) {
	case MinMaxCurveMode::Constant:
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		ImGui::DragFloat("##val", &curve.constant_min, 0.01f);
		break;
	case MinMaxCurveMode::RandomBetweenConstants:
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		ImGui::DragFloat("##min", &curve.constant_min, 0.01f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		ImGui::DragFloat("##max", &curve.constant_max, 0.01f);
		break;
	case MinMaxCurveMode::Curve:
		ImGui::SameLine();
		ImGui::SetNextItemWidth(60);
		ImGui::DragFloat("##scalar", &curve.curve_scalar, 0.01f);
		ImGui::SameLine();
		if (CurveEditorImgui::draw_curve_preview("##preview0", curve.curve0, 80)) {
			editing_curve = &curve;
			show_curve_popup = true;
			curve_editor_popup.clear_all();
			curve_editor_popup.add_curve(curve.curve0);
		}
		break;
	case MinMaxCurveMode::RandomBetweenCurves:
		ImGui::SameLine();
		ImGui::SetNextItemWidth(60);
		ImGui::DragFloat("##scalar", &curve.curve_scalar, 0.01f);
		ImGui::SameLine();
		if (CurveEditorImgui::draw_curve_preview("##preview01", curve.curve0, 80)) {
			editing_curve = &curve;
			show_curve_popup = true;
			curve_editor_popup.clear_all();
			curve_editor_popup.add_curve(curve.curve0);
			curve_editor_popup.add_curve(curve.curve1);
		}
		break;
	}
	ImGui::PopID();
}

void ParticleSystemEditorUi::draw_gradient_field(const char* label, Gradient& gradient)
{
	ImGui::PushID(label);
	ImGui::Text("%s", label);
	ImGui::SameLine(120);
	if (gradient_editor.draw_preview(label, gradient)) {
		editing_gradient = &gradient;
		show_gradient_popup = true;
	}
	ImGui::PopID();
}

bool ParticleSystemEditorUi::draw()
{
	auto* asset = comp->particle_asset.get();
	if (!asset || !asset->is_valid_to_use()) {
		ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No particle asset assigned");
		if (ImGui::Button("Create New...")) {
			show_create_popup = true;
			memset(create_name, 0, sizeof(create_name));
		}
		if (show_create_popup) {
			ImGui::OpenPopup("Create Particle Asset");
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		}
		if (ImGui::BeginPopupModal("Create Particle Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			std::string folder;
			if (AssetBrowser::inst)
				folder = AssetBrowser::inst->selected_folder;
			ImGui::TextDisabled("Folder: %s", folder.empty() ? "(root)" : folder.c_str());
			ImGui::Text("Name (no extension):");
			bool enter = ImGui::InputText("##name", create_name, sizeof(create_name), ImGuiInputTextFlags_EnterReturnsTrue);

			bool do_create = enter || ImGui::Button("Create", ImVec2(120, 0));
			ImGui::SameLine();
			bool do_cancel = ImGui::Button("Cancel", ImVec2(120, 0));

			if (do_create && strlen(create_name) > 0) {
				auto result = AssetTemplates::create_empty_particle(folder, create_name);
				if (result) {
					comp->particle_asset = g_assets.find<ParticleAsset>(*result);
					sys_print(Info, "Created particle asset: %s\n", result->c_str());
				} else {
					sys_print(Warning, "Failed to create particle asset (already exists?)\n");
				}
				show_create_popup = false;
				ImGui::CloseCurrentPopup();
			}
			if (do_cancel) {
				show_create_popup = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		return false;
	}

	draw_playback_controls();
	ImGui::Separator();
	draw_subsystem_list();

	if (selected_subsystem >= 0 && selected_subsystem < (int)asset->subsystems.size()) {
		auto& ss = asset->subsystems[selected_subsystem];
		ImGui::Separator();

		draw_main_module(ss.main);

		bool enabled_emission = ss.emission.enabled;
		if (draw_module_header("Emission", enabled_emission, expanded_emission)) {
			ss.emission.enabled = enabled_emission;
			draw_emission_module(ss.emission);
		} else ss.emission.enabled = enabled_emission;

		bool enabled_shape = ss.shape.enabled;
		if (draw_module_header("Shape", enabled_shape, expanded_shape)) {
			ss.shape.enabled = enabled_shape;
			draw_shape_module(ss.shape);
		} else ss.shape.enabled = enabled_shape;

		bool enabled_vel = ss.velocity_over_lifetime.enabled;
		if (draw_module_header("Velocity over Lifetime", enabled_vel, expanded_velocity)) {
			ss.velocity_over_lifetime.enabled = enabled_vel;
			draw_velocity_module(ss.velocity_over_lifetime);
		} else ss.velocity_over_lifetime.enabled = enabled_vel;

		bool enabled_col = ss.color_over_lifetime.enabled;
		if (draw_module_header("Color over Lifetime", enabled_col, expanded_color)) {
			ss.color_over_lifetime.enabled = enabled_col;
			draw_color_module(ss.color_over_lifetime);
		} else ss.color_over_lifetime.enabled = enabled_col;

		bool enabled_size = ss.size_over_lifetime.enabled;
		if (draw_module_header("Size over Lifetime", enabled_size, expanded_size)) {
			ss.size_over_lifetime.enabled = enabled_size;
			draw_size_module(ss.size_over_lifetime);
		} else ss.size_over_lifetime.enabled = enabled_size;

		bool enabled_rot = ss.rotation_over_lifetime.enabled;
		if (draw_module_header("Rotation over Lifetime", enabled_rot, expanded_rotation)) {
			ss.rotation_over_lifetime.enabled = enabled_rot;
			draw_rotation_module(ss.rotation_over_lifetime);
		} else ss.rotation_over_lifetime.enabled = enabled_rot;

		bool enabled_tex = ss.texture_sheet.enabled;
		if (draw_module_header("Texture Sheet", enabled_tex, expanded_texture)) {
			ss.texture_sheet.enabled = enabled_tex;
			draw_texture_sheet_module(ss.texture_sheet);
		} else ss.texture_sheet.enabled = enabled_tex;

		bool enabled_noise = ss.noise.enabled;
		if (draw_module_header("Noise", enabled_noise, expanded_noise)) {
			ss.noise.enabled = enabled_noise;
			draw_noise_module(ss.noise);
		} else ss.noise.enabled = enabled_noise;

		bool enabled_rend = ss.renderer.enabled;
		if (draw_module_header("Renderer", enabled_rend, expanded_renderer)) {
			ss.renderer.enabled = enabled_rend;
			draw_renderer_module(ss.renderer);
		} else ss.renderer.enabled = enabled_rend;
	}

	comp->update_shape_gizmo(selected_subsystem);

	ImGui::Separator();
	if (ImGui::Button("Save")) {
		asset->save_to_disk();
	}
	ImGui::SameLine();
	if (ImGui::Button("Revert")) {
		g_assets.reload(asset);
		comp->clear();
		comp->play();
	}

	// curve editor popup window
	if (show_curve_popup) {
		ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Curve Editor##ParticlePopup", &show_curve_popup)) {
			curve_editor_popup.draw_content();
			if (editing_curve) {
				auto& curves = curve_editor_popup.get_curve_array();
				if (curves.size() > 0)
					editing_curve->curve0 = curves[0];
				if (curves.size() > 1)
					editing_curve->curve1 = curves[1];
			}
		}
		ImGui::End();
	}

	// gradient editor popup window
	if (show_gradient_popup) {
		ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Gradient Editor##ParticlePopup", &show_gradient_popup)) {
			if (editing_gradient)
				gradient_window_editor.draw_editor("##gradient_edit", *editing_gradient);
		}
		ImGui::End();
	}

	return false;
}

void ParticleSystemEditorUi::draw_playback_controls()
{
	ImGui::BeginGroup();
	if (ImGui::Button("Play"))
		comp->play();
	ImGui::SameLine();
	if (ImGui::Button("Stop")) {
		comp->stop_emitting();
		comp->clear();
	}
	ImGui::SameLine();
	if (ImGui::Button("Restart")) {
		comp->clear();
		comp->play();
	}
	ImGui::SameLine();
	ImGui::Text("Particles: %d", comp->get_alive_count());
	ImGui::EndGroup();
}

void ParticleSystemEditorUi::draw_subsystem_list()
{
	auto* asset = comp->particle_asset.get();
	if (!asset) return;

	ImGui::Text("Subsystems:");
	for (int i = 0; i < (int)asset->subsystems.size(); i++) {
		bool selected = (i == selected_subsystem);
		if (ImGui::Selectable(asset->subsystems[i].name.c_str(), selected))
			selected_subsystem = i;
	}
	if (ImGui::Button("Add Subsystem")) {
		ParticleSubSystem ss;
		ss.name = "SubSystem " + std::to_string(asset->subsystems.size() + 1);
		ss.renderer.material = g_assets.find<MaterialInstance>("eng/default_particle.mi");
		asset->subsystems.push_back(std::move(ss));
		comp->clear();
		comp->play();
	}
	if (asset->subsystems.size() > 1 && selected_subsystem >= 0
		&& selected_subsystem < (int)asset->subsystems.size()) {
		ImGui::SameLine();
		if (ImGui::Button("Remove")) {
			asset->subsystems.erase(asset->subsystems.begin() + selected_subsystem);
			if (selected_subsystem >= (int)asset->subsystems.size())
				selected_subsystem = (int)asset->subsystems.size() - 1;
			comp->clear();
			comp->play();
		}
	}
}

void ParticleSystemEditorUi::draw_main_module(MainModule& mod)
{
	if (ImGui::CollapsingHeader("Main", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Indent();
		ImGui::DragFloat("Duration", &mod.duration, 0.1f, 0.f, 100.f);
		ImGui::Checkbox("Looping", &mod.looping);
		draw_minmax_curve("Start Lifetime", mod.start_lifetime);
		draw_minmax_curve("Start Speed", mod.start_speed);
		draw_minmax_curve("Start Size", mod.start_size);
		draw_minmax_curve("Start Rotation", mod.start_rotation);
		draw_gradient_field("Start Color", mod.start_color);
		draw_minmax_curve("Gravity", mod.gravity_modifier);

		const char* sim_space_names[] = {"Local", "World"};
		int sim_space = (int)mod.simulation_space;
		if (ImGui::Combo("Simulation Space", &sim_space, sim_space_names, 2))
			mod.simulation_space = (SimulationSpace)sim_space;

		ImGui::DragInt("Max Particles", &mod.max_particles, 1, 1, 10000);
		ImGui::Checkbox("Play on Awake", &mod.play_on_awake);
		ImGui::Unindent();
	}
}

void ParticleSystemEditorUi::draw_emission_module(EmissionModule& mod)
{
	ImGui::Indent();
	draw_minmax_curve("Rate over Time", mod.rate_over_time);

	ImGui::Text("Bursts:");
	for (int i = 0; i < (int)mod.bursts.size(); i++) {
		ImGui::PushID(i);
		auto& b = mod.bursts[i];
		ImGui::DragFloat("Time", &b.time, 0.01f, 0.f, 100.f);
		draw_minmax_curve("Count", b.count);
		ImGui::DragInt("Cycles", &b.cycles, 1, 1, 100);
		ImGui::DragFloat("Interval", &b.interval, 0.01f, 0.001f, 10.f);
		if (ImGui::Button("Remove Burst")) {
			mod.bursts.erase(mod.bursts.begin() + i);
			ImGui::PopID();
			break;
		}
		ImGui::Separator();
		ImGui::PopID();
	}
	if (ImGui::Button("Add Burst"))
		mod.bursts.push_back(EmissionBurst());
	ImGui::Unindent();
}

void ParticleSystemEditorUi::draw_shape_module(ShapeModule& mod)
{
	ImGui::Indent();
	const char* shape_names[] = {"Sphere", "Hemisphere", "Cone", "Box", "Circle"};
	int shape = (int)mod.shape;
	if (ImGui::Combo("Shape", &shape, shape_names, 5))
		mod.shape = (ParticleShapeType)shape;

	ImGui::DragFloat("Radius", &mod.radius, 0.01f, 0.f, 100.f);
	if (mod.shape == ParticleShapeType::Cone)
		ImGui::DragFloat("Angle", &mod.angle, 0.5f, 0.f, 90.f);
	if (mod.shape == ParticleShapeType::Circle)
		ImGui::DragFloat("Arc", &mod.arc, 1.f, 0.f, 360.f);
	ImGui::DragFloat("Radius Thickness", &mod.radius_thickness, 0.01f, 0.f, 1.f);
	ImGui::DragFloat3("Position", &mod.position_offset.x, 0.01f);
	ImGui::DragFloat3("Rotation", &mod.rotation_offset.x, 0.5f);
	ImGui::DragFloat3("Scale", &mod.scale_offset.x, 0.01f);
	ImGui::Unindent();
}

void ParticleSystemEditorUi::draw_velocity_module(VelocityOverLifetimeModule& mod)
{
	ImGui::Indent();
	draw_minmax_curve("X", mod.x);
	draw_minmax_curve("Y", mod.y);
	draw_minmax_curve("Z", mod.z);

	const char* space_names[] = {"Local", "World"};
	int space = (int)mod.space;
	if (ImGui::Combo("Space", &space, space_names, 2))
		mod.space = (SimulationSpace)space;

	draw_minmax_curve("Orbital X", mod.orbital_x);
	draw_minmax_curve("Orbital Y", mod.orbital_y);
	draw_minmax_curve("Orbital Z", mod.orbital_z);
	draw_minmax_curve("Radial", mod.radial);
	ImGui::Unindent();
}

void ParticleSystemEditorUi::draw_color_module(ColorOverLifetimeModule& mod)
{
	ImGui::Indent();
	draw_gradient_field("Color", mod.color);
	ImGui::Unindent();
}

void ParticleSystemEditorUi::draw_size_module(SizeOverLifetimeModule& mod)
{
	ImGui::Indent();
	ImGui::Checkbox("Separate Axes", &mod.separate_axes);
	if (mod.separate_axes) {
		draw_minmax_curve("X", mod.x);
		draw_minmax_curve("Y", mod.y);
		draw_minmax_curve("Z", mod.z);
	}
	else {
		draw_minmax_curve("Size", mod.size);
	}
	ImGui::Unindent();
}

void ParticleSystemEditorUi::draw_rotation_module(RotationOverLifetimeModule& mod)
{
	ImGui::Indent();
	ImGui::Checkbox("Separate Axes", &mod.separate_axes);
	draw_minmax_curve("Angular Velocity", mod.angular_velocity);
	ImGui::Unindent();
}

void ParticleSystemEditorUi::draw_texture_sheet_module(TextureSheetModule& mod)
{
	ImGui::Indent();
	ImGui::DragInt("Tiles X", &mod.tiles_x, 1, 1, 32);
	ImGui::DragInt("Tiles Y", &mod.tiles_y, 1, 1, 32);

	const char* anim_names[] = {"Whole Sheet", "Single Row"};
	int anim = (int)mod.animation;
	if (ImGui::Combo("Animation", &anim, anim_names, 2))
		mod.animation = (TextureSheetAnimation)anim;

	draw_minmax_curve("Frame over Time", mod.frame_over_time);
	draw_minmax_curve("Start Frame", mod.start_frame);
	ImGui::DragInt("Cycles", &mod.cycles, 1, 1, 100);
	ImGui::Unindent();
}

void ParticleSystemEditorUi::draw_noise_module(NoiseModule& mod)
{
	ImGui::Indent();
	draw_minmax_curve("Strength", mod.strength);
	ImGui::DragFloat("Frequency", &mod.frequency, 0.01f, 0.01f, 10.f);
	draw_minmax_curve("Scroll Speed", mod.scroll_speed);
	ImGui::DragInt("Octaves", &mod.octaves, 1, 1, 8);
	ImGui::Checkbox("Damping", &mod.damping);

	const char* quality_names[] = {"Low", "Medium", "High"};
	int q = (int)mod.quality;
	if (ImGui::Combo("Quality", &q, quality_names, 3))
		mod.quality = (NoiseQuality)q;
	ImGui::Unindent();
}

void ParticleSystemEditorUi::draw_renderer_module(RendererModule& mod)
{
	ImGui::Indent();

	// Material drag-drop target
	const char* mat_name = mod.material.get() ? mod.material.get()->get_name().c_str() : "<none>";
	ImGui::Text("Material");
	ImGui::SameLine(120);
	ImGui::Button(mat_name, ImVec2(ImGui::GetContentRegionAvail().x, 0));
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Drag and drop a Material asset here");
	if (ImGui::BeginDragDropTarget()) {
		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop", ImGuiDragDropFlags_AcceptPeekOnly);
		if (payload) {
			AssetOnDisk* resource = *(AssetOnDisk**)payload->Data;
			auto* metadata = AssetRegistrySystem::get().find_for_classtype(&MaterialInstance::StaticType);
			if (resource->type == metadata) {
				if ((payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop"))) {
					mod.material = g_assets.find<MaterialInstance>(resource->filename);
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	const char* render_mode_names[] = {"Billboard", "Stretched Billboard", "Mesh"};
	int rm = (int)mod.render_mode;
	if (ImGui::Combo("Render Mode", &rm, render_mode_names, 3))
		mod.render_mode = (ParticleRenderMode)rm;

	const char* sort_names[] = {"None", "By Distance", "Oldest First", "Youngest First"};
	int sm = (int)mod.sort_mode;
	if (ImGui::Combo("Sort Mode", &sm, sort_names, 4))
		mod.sort_mode = (ParticleSortMode)sm;

	if (mod.render_mode == ParticleRenderMode::StretchedBillboard) {
		ImGui::DragFloat("Speed Scale", &mod.speed_scale, 0.01f);
		ImGui::DragFloat("Length Scale", &mod.length_scale, 0.01f);
	}
	ImGui::Unindent();
}

std::unique_ptr<IComponentEditorUi> ParticleSystemComponent::create_editor_ui()
{
	return std::make_unique<ParticleSystemEditorUi>(this);
}

#endif
