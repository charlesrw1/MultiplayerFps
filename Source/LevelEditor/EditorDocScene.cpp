// Scene object management, DrawHandlesObject, EntityVisiblityFilter, and API impls.
// Logical split from EditorDocLocal.cpp.
#ifdef EDITOR_BUILD
#include "EditorDocLocal.h"
#include "imgui.h"
#include "Input/InputSystem.h"
#include "Render/DrawPublic.h"
#include "UI/GUISystemPublic.h"
#include "Game/EntityComponent.h"
#include "Game/Components/LightComponents.h"
#include "Game/Components/DecalComponent.h"
#include "Game/Components/SpawnerComponenth.h"
#include "Debug.h"
#include "Commands.h"
#include "Assets/AssetDatabase.h"
#include "Render/MaterialPublic.h"
#include "Framework/Config.h"

// ---------------------------------------------------------------------------
// Scene entity helpers
// ---------------------------------------------------------------------------

Entity* EditorDoc::spawn_entity() {
	ASSERT(eng->get_level());
	Entity* e = eng->get_level()->spawn_entity();
	instantiate_into_scene(e);
	return e;
}

Component* EditorDoc::attach_component(const ClassTypeInfo* ti, Entity* e) {
	ASSERT(ti);
	ASSERT(e);
	Component* c = e->create_component(ti);
	instantiate_into_scene(c);
	return c;
}

void EditorDoc::remove_scene_object(BaseUpdater* u) {
	ASSERT(u);
	u->destroy_deferred();
}

void EditorDoc::insert_unserialized_into_scene(UnserializedSceneFile& file) {
	ASSERT(eng->get_level());
	eng->get_level()->insert_unserialized_entities_into_level(file);
}

void EditorDoc::instantiate_into_scene(BaseUpdater* u) {
	ASSERT(u || !u); // u may legitimately be null in some call paths
}

// ---------------------------------------------------------------------------
// DrawHandlesObject
// ---------------------------------------------------------------------------

// Live-tunable dashed-line params (exposed in the "Parent Lines" debug menu below).
static float g_dashed_half_w_px = 16.f;   // on-screen half-thickness of the ribbon, in pixels
static float g_dashed_period_px = 250.f;   // on-screen length of one dash+gap cell, in pixels

static void draw_dashed_line_debug_menu() {
	ImGui::SliderFloat("thickness (px)", &g_dashed_half_w_px, 1.f, 50.f, "%.2f");
	ImGui::SliderFloat("dash period (px)", &g_dashed_period_px, 20.f, 500.f, "%.1f");
	ImGui::TextDisabled("prefab-edit relationship lines");
}
static AddToDebugMenu s_dashed_line_menu("Parent Lines", draw_dashed_line_debug_menu);

// Builds one camera-facing dashed ribbon (a stretched textured quad) from `from` to `to`. Both the
// UV.x tiling and the ribbon width are driven off the line's *screen-space* size so the dash sprite
// stays a constant pixel length and the ribbon a constant pixel thickness regardless of distance:
// UV.x is tiled 0..N where N = on-screen length / dash-period-in-pixels, and the world half-width is
// whatever projects to a fixed pixel thickness at the segment's distance.
static void push_dashed_ribbon(MeshBuilder& mb, glm::vec3 from, glm::vec3 to, const View_Setup& view) {
	glm::vec3 along = to - from;
	float len = glm::length(along);
	if (len < 1e-4f)
		return;
	along /= len;
	glm::vec3 mid = 0.5f * (from + to);
	glm::vec3 to_cam = view.origin - mid;
	// Distance from the eye to the segment. Euclidean (not view-space depth) so it is always positive
	// and never flips scale for segments off to the side or straddling the near plane.
	float cam_dist = glm::length(to_cam);
	if (cam_dist < 1e-4f)
		return;
	// Isotropic pixels-per-world: proj[1][1] = cot(fovy/2) in perspective, or 2/ortho_height in ortho,
	// and for square pixels the x density (proj[0][0]*w/2) equals the y density (proj[1][1]*h/2), so
	// one factor covers both. Perspective divides by eye distance (screen size shrinks with depth);
	// ortho does not -- the projection matrix already maps world units to screen space independent of
	// depth, so dividing by cam_dist there would make the ribbon shrink/balloon with camera distance
	// instead of staying a constant on-screen thickness.
	const float px_per_world = view.is_ortho
		? (view.proj[1][1] * 0.5f * (float)view.height)
		: (view.proj[1][1] * 0.5f * (float)view.height) / cam_dist;
	if (px_per_world < 1e-6f)
		return;
	glm::vec3 side = glm::cross(along, to_cam);
	float sl = glm::length(side);
	if (sl < 1e-5f)
		return; // viewing straight down the line — nothing sensible to draw
	const float half_w = g_dashed_half_w_px / px_per_world;
	side *= (half_w / sl);
	const float u_max = (len * px_per_world) / glm::max(g_dashed_period_px, 1e-3f);
	const Color32 col = COLOR_WHITE; // color comes from the sprite texture
	const int base = mb.GetBaseVertex();
	MbVertex v0(from - side, col); v0.uv = glm::vec2(0.f, 0.f);
	MbVertex v1(from + side, col); v1.uv = glm::vec2(0.f, 1.f);
	MbVertex v2(to + side, col);   v2.uv = glm::vec2(u_max, 1.f);
	MbVertex v3(to - side, col);   v3.uv = glm::vec2(u_max, 0.f);
	mb.AddVertex(v0);
	mb.AddVertex(v1);
	mb.AddVertex(v2);
	mb.AddVertex(v3);
	mb.AddQuad(base, base + 1, base + 2, base + 3);
}

DrawHandlesObject::~DrawHandlesObject() {
	if (parent_line_handle.is_valid() && idraw && idraw->get_scene())
		idraw->get_scene()->remove_particle_obj(parent_line_handle);
}

void DrawHandlesObject::tick_parent_lines() {
	if (!tried_load_dashed_mat) {
		tried_load_dashed_mat = true;
		dashed_mat = g_assets.find<MaterialInstance>("eng/dashed_line.mi").get();
	}
	if (!parent_line_handle.is_valid())
		parent_line_handle = idraw->get_scene()->register_particle_obj();

	// Rebuild the ribbon every frame. Parenting is prefab-only, so outside prefab-edit mode the
	// meshbuilder is left empty and nothing is drawn (the particle path just uploads zero indices).
	parent_line_mb.Begin();
	if (doc.is_editing_prefab()) {
		if (Level* level = eng->get_level()) {
			for (auto obj : level->get_all_objects()) {
				Entity* e = obj->cast_to<Entity>();
				if (!e || !e->get_parent())
					continue;
				const glm::vec3 child_p = e->get_ws_position();
				// get_parent_transform() resolves the bone attach point when parented to a bone,
				// otherwise the parent entity's world transform.
				const glm::vec3 parent_p = glm::vec3(e->get_parent_transform()[3]);
				push_dashed_ribbon(parent_line_mb, child_p, parent_p, doc.vs_setup);
			}
		}
	}
	parent_line_mb.End();

	Particle_Object po;
	po.meshbuilder = &parent_line_mb;
	po.material = dashed_mat; // null falls back to particle_default.mm (flat vertex colour)
	po.transform = glm::mat4(1.f);
	idraw->get_scene()->update_particle_obj(parent_line_handle, po);
}

void DrawHandlesObject::tick() {
	ASSERT(&doc);

	tick_parent_lines();

	// draw gizmos for selected entities' components
	for (auto& sel : doc.selection_state->get_selection_as_vector()) {
		if (auto* e = sel.get()) {
			for (auto* comp : e->get_components())
				comp->editor_on_draw_gizmos_selected();
		}
	}

	int box_mode = ed_show_box_handles.get_integer();
	if (box_mode == 0)
		return;

	BoxHandleMode handle_mode = (box_mode == 1) ? BoxHandleMode::Face : BoxHandleMode::Edge;

	if (doc.selection_state->has_only_one_selected()) {
		last_selected = doc.selection_state->get_only_one_selected();
	} else {
		last_selected = EntityPtr();
	}

	if (!last_selected.get()) {
		last_selected = EntityPtr();
		return;
	}

	Bounds bounds_to_use;
	bool good_to_use = false;

	if (auto mesh = last_selected->get_component<MeshComponent>(); mesh && mesh->get_model()) {
		bounds_to_use = mesh->get_model()->get_bounds();
		good_to_use = true;
	} else if (last_selected->get_component<CubemapComponent>() || last_selected->get_component<DecalComponent>()) {
		bounds_to_use = Bounds(-vec3(0.5), vec3(0.5));
		good_to_use = true;
	}

	if (!good_to_use)
		return;

	auto transform = last_selected->get_ws_transform();
	glm::mat4 m = transform * glm::translate(glm::mat4(1), bounds_to_use.bmin);
	auto extents = bounds_to_use.bmax - bounds_to_use.bmin;
	auto result = doc.handle_dragger->box_handles(1, m, extents, handle_mode);

	if (result == VHResult::Changing && !was_dragging) {
		pre_drag_transform = transform;
		was_dragging = true;
	}

	if (result == VHResult::Changing || result == VHResult::Finished) {
		mat4 want = m * glm::inverse(glm::translate(glm::mat4(1), bounds_to_use.bmin));
		glm::vec3 p, s;
		glm::quat q;
		decompose_transform(want, p, q, s);
		q = glm::normalize(q);
		want = compose_transform(p, q, s);
		last_selected->set_ws_transform(want);
		doc.manipulate->update_pivot_and_cached();
	}

	if (result == VHResult::Finished && was_dragging) {
		uint64_t id = last_selected->get_instance_id();
		std::unordered_set<uint64_t> sel_set = {id};
		std::unordered_map<uint64_t, glm::mat4> pre_map = {{id, pre_drag_transform}};
		doc.command_mgr->add_command(new TransformCommand(doc, sel_set, pre_map));
		was_dragging = false;
	}

	Color32 box_color = (handle_mode == BoxHandleMode::Face) ? Color32(255, 165, 0) : Color32(255, 220, 80);
	Debug::add_transformed_box(m, extents, box_color, 0, false);
}

// ---------------------------------------------------------------------------
// EntityVisiblityFilter
// ---------------------------------------------------------------------------

void EntityVisiblityFilter::tick() {
	ASSERT(&doc);
	if (!ImGui::Begin("OutlineFilter")) {
		ImGui::End();
		return;
	}

	// not a pretty way
	auto draw_item = [&](const string& s) -> int {
		if (!MapUtil::contains(status, s))
			status[s] = true;
		bool b = status[s];
		if (ImGui::Selectable(s.c_str(), false, 0, ImVec2(200, 0)))
			return 1;
		ImGui::SameLine();
		ImGui::PushID(s.c_str());
		if (ImGui::Checkbox("##empty", &b)) {
			status[s] = b;
			ImGui::PopID();
			return b ? 2 : 3;
		}
		ImGui::PopID();
		return 0;
	};
	auto add_component_type_to_selection = [&](const ClassTypeInfo& t) {
		auto& objs = eng->get_level()->get_all_objects();
		const bool select_only = !Input::is_shift_down();
		if (select_only)
			doc.selection_state->clear_all_selected();
		for (auto o : objs) {
			if (o->get_type().is_a(t)) {
				auto owner = o->cast_to<Component>()->get_owner();
				doc.selection_state->add_to_entity_selection(owner);
			}
		}
	};
	auto set_component_visibility = [&](const ClassTypeInfo& t, bool b) {
		auto& objs = eng->get_level()->get_all_objects();
		for (auto o : objs) {
			if (o->get_type().is_a(t)) {
				auto owner = o->cast_to<Component>()->get_owner();
				owner->set_hidden_in_editor(!b);
			}
		}
	};
	auto do_stuff = [&](const ClassTypeInfo& t, int res) {
		if (res == 1)
			add_component_type_to_selection(t);
		else if (res == 2)
			set_component_visibility(t, true);
		else if (res == 3)
			set_component_visibility(t, false);
	};

	int res = draw_item("Lights");
	do_stuff(PointLightComponent::StaticType, res);
	do_stuff(SpotLightComponent::StaticType, res);
	res = draw_item("GiVols");
	do_stuff(GiVolumeComponent::StaticType, res);
	res = draw_item("CubemapVols");
	do_stuff(CubemapComponent::StaticType, res);
	res = draw_item("Decals");
	do_stuff(DecalComponent::StaticType, res);
	res = draw_item("Sun");
	do_stuff(SunLightComponent::StaticType, res);
	res = draw_item("Env");
	do_stuff(SkylightComponent::StaticType, res);
	res = draw_item("Spawners");
	do_stuff(SpawnerComponent::StaticType, res);
	ImGui::End();
}

// ---------------------------------------------------------------------------
// IEditorTool factory
// ---------------------------------------------------------------------------

IEditorTool* IEditorTool::create(string mapname) {
	ASSERT(!mapname.empty() || mapname.empty()); // mapname may be empty for new scenes
	return EditorDoc::create_scene(mapname);
}

// ---------------------------------------------------------------------------
// ISelectionApi helpers
// ---------------------------------------------------------------------------

void ISelectionApi::do_selection(MouseSelectionAction action, std::vector<EntityPtr> ptrs) {
	ASSERT(ptrs.empty() || !ptrs.empty()); // always valid to pass an empty list
	if (action == MouseSelectionAction::ADD_SELECT) {
		for (EntityPtr p : ptrs)
			add_select(p);
	} else if (action == MouseSelectionAction::SELECT_ONLY) {
		clear_selected();
		for (EntityPtr p : ptrs)
			add_select(p);
	} else {
		for (EntityPtr p : ptrs)
			remove_select(p);
	}
}

// ---------------------------------------------------------------------------
// DocumentApiImpl
// ---------------------------------------------------------------------------

void DocumentApiImpl::save() {
	ASSERT(doc);
	doc->save();
}

void DocumentApiImpl::undo() {
	ASSERT(doc);
	if (doc->command_mgr)
		doc->command_mgr->undo();
}

void DocumentApiImpl::redo() {
	ASSERT(doc);
	// TODO: Implement redo when UndoRedoSystem supports it
}

std::string DocumentApiImpl::get_document_name() const {
	ASSERT(doc);
	return doc->get_doc_name();
}

#endif // EDITOR_BUILD
