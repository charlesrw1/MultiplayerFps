#include "EditorModes.h"
#include "EditorDocLocal.h"
#include "Commands.h"
#include "Game/Components/TerrainComponent.h"
#include "Input/InputSystem.h"
#include "Physics/Physics2.h"
#include "Framework/MeshBuilder.h"
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"

namespace {
class TerrainEditCommand final : public Command {
public:
	TerrainEditCommand(TerrainComponent* terrain, std::vector<float> before_h, std::vector<uint8_t> before_w,
		std::vector<float> after_h, std::vector<uint8_t> after_w)
		: terrain(terrain), before_h(std::move(before_h)), before_w(std::move(before_w)), after_h(std::move(after_h)), after_w(std::move(after_w)) {}
	void execute() override { if (terrain) terrain->restore_edit_data(after_h, after_w); }
	void undo() override { if (terrain) terrain->restore_edit_data(before_h, before_w); }
	std::string to_string() override { return "Terrain Brush Stroke"; }
private:
	TerrainComponent* terrain;
	std::vector<float> before_h; std::vector<uint8_t> before_w;
	std::vector<float> after_h; std::vector<uint8_t> after_w;
};

void draw_brush_ring(MeshBuilder& mesh, TerrainComponent& terrain, const glm::vec3& centre, float radius, float falloff) {
	mesh.Begin(128);
	constexpr int segments = 64;
	auto ring = [&](float ring_radius, Color32 color) {
		for (int i=0; i<segments; ++i) {
			const float a0=glm::two_pi<float>()*i/segments, a1=glm::two_pi<float>()*(i+1)/segments;
			glm::vec3 p0(centre.x+cosf(a0)*ring_radius,0,centre.z+sinf(a0)*ring_radius);
			glm::vec3 p1(centre.x+cosf(a1)*ring_radius,0,centre.z+sinf(a1)*ring_radius);
			p0.y=terrain.sample_height_world(p0.x,p0.z)+0.06f; p1.y=terrain.sample_height_world(p1.x,p1.z)+0.06f;
			mesh.PushLine(p0,p1,color);
		}
	};
	ring(radius, COLOR_GREEN);
	// This inner line shows the 50% influence contour for the selected falloff.
	ring(radius*(1.f-std::pow(.5f,1.f/glm::max(falloff,.01f))), Color32(255,220,80,220));
	mesh.End();
}
}

TerrainEditorTool::~TerrainEditorTool() {
	if (brush_preview_handle.is_valid()) idraw->get_scene()->remove_meshbuilder(brush_preview_handle);
	delete brush_preview;
}

void TerrainEditorTool::draw_ui() {
	if (!ImGui::Begin("Terrain Editor")) { ImGui::End(); return; }
	const char* brushes[] = {"Raise", "Lower", "Smooth", "Flatten", "Noise", "Thermal Erosion", "Paint Material"};
	ImGui::Combo("Brush", &brush, brushes, IM_ARRAYSIZE(brushes));
	ImGui::DragFloat("Radius", &radius, 0.25f, 0.25f, 200.f, "%.2f m");
	ImGui::DragFloat("Strength", &strength, 0.02f, 0.001f, 50.f);
	ImGui::SliderFloat("Falloff", &falloff, 0.1f, 8.f, "%.2f");
	if (brush == (int)TerrainComponent::Brush::PaintMaterial) ImGui::SliderInt("Material Layer", &material_layer, 0, 3);
	EntityPtr active = doc.selection_state->get_active(); TerrainComponent* terrain = active ? active->get_component<TerrainComponent>() : nullptr;
	if (!terrain) ImGui::TextDisabled("Select an entity with a TerrainComponent to sculpt it.");
	else { ImGui::Text("%d x %d heightmap, %.0f m wide", terrain->get_resolution(), terrain->get_resolution(), terrain->get_size()); if (ImGui::Button("Save Terrain Data")) terrain->save_data(); }
	ImGui::TextDisabled("LMB paints in the viewport. Each stroke is undoable.");
	ImGui::End();
}

void TerrainEditorTool::tick(EditorInputs& inputs) {
	EntityPtr active = doc.selection_state->get_active(); TerrainComponent* terrain = active ? active->get_component<TerrainComponent>() : nullptr;
	const bool down=Input::is_mouse_down(0);
	if (!down && stroke_terrain) { stroke_terrain->finish_edit_stroke(); doc.command_mgr->add_command(new TerrainEditCommand(stroke_terrain,std::move(stroke_before_heights),std::move(stroke_before_weights),stroke_terrain->copy_heights(),stroke_terrain->copy_weights())); stroke_terrain=nullptr; }
	if (!terrain || !ViewportSystem::is_vp_hovered()) { if (brush_preview_handle.is_valid()) { MeshBuilder_Object o; o.visible=false; idraw->get_scene()->update_meshbuilder(brush_preview_handle,o); } return; }
	auto mouse=Input::get_mouse_pos(); Ray ray=doc.unproject_mouse_to_ray(mouse.x,mouse.y); world_query_result hit;
	if (!g_physics.trace_ray(hit,doc.get_vs()->origin,doc.get_vs()->origin-ray.dir*10000.f,nullptr,UINT32_MAX)) return;
	if (!brush_preview) brush_preview=new MeshBuilder;
	draw_brush_ring(*brush_preview,*terrain,hit.hit_pos,radius,falloff);
	if (!brush_preview_handle.is_valid()) brush_preview_handle=idraw->get_scene()->register_meshbuilder();
	MeshBuilder_Object preview; preview.meshbuilder=brush_preview; preview.visible=true; preview.depth_tested=true; idraw->get_scene()->update_meshbuilder(brush_preview_handle,preview);
	if (!down || !inputs.can_use_mouse_click()) return;
	if (!stroke_terrain) { stroke_terrain=terrain; stroke_before_heights=terrain->copy_heights(); stroke_before_weights=terrain->copy_weights(); }
	stroke_terrain->apply_brush((TerrainComponent::Brush)brush,hit.hit_pos,radius,strength*eng->get_dt(),falloff,material_layer);
}
