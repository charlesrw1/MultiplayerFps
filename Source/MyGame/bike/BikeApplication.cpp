#include "BikeHeaders.h"

#include "Render/Texture.h"
#include "Render/Model.h"
#include "Game/GameplayStatic.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/DecalComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Game/Entities/CharacterController.h"
#include "Input/InputSystem.h"
#include "GameEnginePublic.h"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "imgui.h"
#include "Input/Sdl2CompatGamepad.h"
#include <glm/gtc/matrix_transform.hpp>
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "UI/Gui.h"
#include "Debug.h"
#include <algorithm>
#include <fstream>
#include <random>

// Wind state is accessed via the global WindSystem: g_wind.wind_speed, etc.

// Debug pointers (set each frame in evaluate; bp_for_debug is non-static so BikeCamera.cpp can extern it)
BikeObject* bo_for_debug = nullptr;
BikePlayer*        bp_for_debug = nullptr;
BikeGameApplication* g_bike_app = nullptr;
BikeAIParams g_ai_params;

// Forward declare — defined in BikeApplication_Debug.cpp
void apply_debug_follow_camera();
extern bool g_follow_rider;

// Forward declare — defined in BikeApplication_AIDebug.cpp
void ai_debug_update(BikeGameApplication* app, float dt);

// ============================================================
// Crack type registry — add rows here for crack2, crack3, etc.
// ============================================================

struct CrackTypeConfig {
	const char* material_path;
	float strength;     // base impulse magnitude (speed-scaled at trigger time)
	float radius_mult;  // multiplier on the decal's natural XZ footprint for the trigger zone
	float cooldown_dist;  // metres to travel before same crack can retrigger (speed-invariant)
	int   count;        // populated by collect_crack_decals, read-only
};

static constexpr int NUM_CRACK_TYPES = 1;

// Per-type crack configuration (add more rows for crack2, crack3, etc.)
CrackTypeConfig crack_types[NUM_CRACK_TYPES] = {
	{ "materials/decals/crack1_decal.mi", 0.5f, 1.0f, 0.8f, 0 },
	//  material                          str   rmul  dist  count
};

// Speed scaling: impulse = strength * clamp(speed / speed_ref, speed_min_mult, speed_max_mult)
static float crack_speed_ref      = 10.f;   // m/s at which mult = 1.0
static float crack_speed_min_mult = 0.2f;   // minimum multiplier (stationary)
static float crack_speed_max_mult = 2.5f;   // cap at high speed

static bool  crack_debug_draw = false;

// ============================================================
// BikeGameApplication
// ============================================================

void BikeGameApplication::collect_crack_decals()
{
	crack_instances.clear();
	auto components = GameplayStatic::find_components(&DecalComponent::StaticType);
	for (auto* c : components) {
		auto* dc = static_cast<DecalComponent*>(c);
		const std::string& path = dc->get_material_path();
		for (int ti = 0; ti < NUM_CRACK_TYPES; ++ti) {
			if (path == crack_types[ti].material_path) {
				// Use the decal's XZ world-space footprint as the natural trigger size.
				// Decals are unit-box projections, so XZ scale = real-world extent in metres.
				const glm::vec3 ws_scale = dc->get_owner()->get_ws_scale();
				const float footprint_r  = glm::max(ws_scale.x, ws_scale.z) * 0.5f;
				const float trigger_r    = footprint_r * crack_types[ti].radius_mult;
				crack_instances.push_back({ dc->get_owner()->get_ws_position(), trigger_r, ti });
				crack_types[ti].count++;
				break;
			}
		}
	}
	sys_print(Debug, "BikeApp: collected %d crack decal(s)\n", (int)crack_instances.size());
}

void BikeGameApplication::start()
{
	GameplayStatic::change_level("bike/bike_test_map.tmap");

	build_hardcoded_circuit(course);
	build_road_mesh();

	collect_crack_decals();
	debugger.init();

	// Spawn finish line prop at the course start/finish position
	if (course.is_built) {
		const BikeWaypoint start_wp = course.sample(0.f);
		const float yaw = std::atan2(start_wp.forward.x, start_wp.forward.z)+PI/2;
		Entity* finish_e = GameplayStatic::spawn_entity();
		finish_e->set_ws_position_rotation(start_wp.position,
			glm::angleAxis(yaw, glm::vec3(0.f, 1.f, 0.f)));
		finish_e->create_component<MeshComponent>()->set_model(
			Model::load("props/race_props/finish_line.cmdl"));
	}

	// AI-only: spawn num_ai riders staggered behind the course start. Player
	// support is re-added later by calling create_player() here too.
	const glm::vec3 start_pos = course.is_built
		? course.sample(0.f).position
		: glm::vec3(0.f);

	static constexpr float AI_GAP_M      = 5.f;   // spacing along course (m)
	static constexpr float AI_LAT_SPREAD = 1.2f;  // lateral spread so they don't all overlap
	for (int i = 0; i < num_ai; ++i) {
		const float dist   = -(i + 1) * AI_GAP_M;  // behind the start line
		glm::vec3   pos    = start_pos;
		if (course.is_built) {
			const BikeWaypoint wp = course.sample(
				glm::mod(dist + course.total_length_m, course.total_length_m));
			// Spread laterally so separation forces have something to work against
			const float lat = ((i % 3) - 1) * AI_LAT_SPREAD * 0.5f;
			pos = wp.position + wp.right * lat;
		}
		create_ai(pos);
	}
}

void BikeGameApplication::rebuild_course()
{
	build_hardcoded_circuit(course);
	build_road_mesh();
	if (draw_racing_line_debug)
		set_draw_racing_line(true);
}

void BikeGameApplication::respawn_ai()
{
	// Destroy all non-player AI riders and remove them from the rider lists.
	for (auto it = all_riders.begin(); it != all_riders.end(); ) {
		BikeObject* bo = *it;
		if (dynamic_cast<BikeAI*>(bo->input.get())) {
			bo->get_owner()->destroy();
			it = all_riders.erase(it);
		} else {
			++it;
		}
	}
	riders_sorted.erase(
		std::remove_if(riders_sorted.begin(), riders_sorted.end(),
			[&](BikeObject* r) {
				for (auto* a : all_riders)
					if (a == r) return false;
				return true;
			}),
		riders_sorted.end());

	// Re-spawn the requested number of AI behind the player.
	const glm::vec3 start_pos = all_riders.empty()
		? glm::vec3(0.f)
		: all_riders[0]->get_ws_position();

	static constexpr float AI_GAP_M      = 5.f;
	static constexpr float AI_LAT_SPREAD = 1.2f;
	for (int i = 0; i < num_ai; ++i) {
		const float dist = -(i + 1) * AI_GAP_M;
		glm::vec3   pos  = start_pos;
		if (course.is_built) {
			const float d  = glm::mod(dist + course.total_length_m, course.total_length_m);
			const BikeWaypoint wp = course.sample(d);
			const float lat = ((i % 3) - 1) * AI_LAT_SPREAD * 0.5f;
			pos = wp.position + wp.right * lat;
		}
		create_ai(pos);
	}
}

void BikeGameApplication::update()
{
	GameplayStatic::reset_debug_text_height();

	g_bike_app = this;
	if (course.is_built) {
		sort_riders();
		update_groups();
		update_drafting();
	}
	update_crack_triggers();
	apply_debug_follow_camera();
	debugger.update(all_riders);

	ai_debug_update(this, eng->get_dt());
}

void BikeGameApplication::on_imgui()
{
	debugger.on_imgui();
}

void BikeGameApplication::sort_riders()
{
	riders_sorted = all_riders;
	std::sort(riders_sorted.begin(), riders_sorted.end(),
		[](const BikeObject* a, const BikeObject* b) {
			return a->course_dist_m > b->course_dist_m;
		});
	for (int i = 0; i < (int)riders_sorted.size(); ++i)
		riders_sorted[i]->race_position = i + 1;
}

void BikeGameApplication::debug_draw_course() const
{
	course.debug_draw();
}

BikeObject* BikeGameApplication::create_player(glm::vec3 pos)
{
	Entity* e = GameplayStatic::spawn_entity();
	e->set_ws_position(pos);
	auto bo = e->create_component<BikeObject>();
	bo->input  = std::make_unique<BikePlayer>();
	bo->course = &course;
	// One-time projection: seeds the rail-authoritative course_dist_m/lateral_pos
	// from the spawn world position. Never called again after this — position is
	// derived FROM these fields from here on (see BikeObject::tick_transform).
	if (course.is_built)
		bo->course_dist_m = course.project(pos, &bo->lateral_pos, &bo->course_segment);
	all_riders.push_back(bo);
	riders_sorted.push_back(bo);
	return bo;
}

BikeObject* BikeGameApplication::create_ai(glm::vec3 pos)
{
	Entity* e = GameplayStatic::spawn_entity();
	e->set_ws_position(pos);
	auto bo = e->create_component<BikeObject>();
	bo->course = &course;

	{
		auto ai    = std::make_unique<BikeAI>();
		ai->course = &course;

		bo->input = std::move(ai);
	}

	// One-time projection: seeds the rail-authoritative course_dist_m/lateral_pos
	// from the spawn world position (see note in create_player).
	if (course.is_built)
		bo->course_dist_m = course.project(pos, &bo->lateral_pos, &bo->course_segment);

	all_riders.push_back(bo);
	riders_sorted.push_back(bo);
	return bo;
}

// ============================================================
// Crack trigger: fire bump FX when a rider rolls over a crack decal
// ============================================================

void BikeGameApplication::update_crack_triggers()
{
	if (crack_instances.empty()) return;
	const float dt = eng->get_dt();

	for (auto* bike : all_riders) {
		if (bike->crack_cooldown > 0.f) {
			bike->crack_cooldown -= dt;
			bike->crack_impulse = 0.f;
			continue;
		}
		bike->crack_impulse = 0.f;

		const glm::vec3 bpos = bike->get_owner()->get_ws_position();
		for (const auto& ci : crack_instances) {
			const CrackTypeConfig& cfg = crack_types[ci.type_idx];
			const float r2 = ci.trigger_radius * ci.trigger_radius;
			const float dx = bpos.x - ci.pos.x;
			const float dz = bpos.z - ci.pos.z;
			if (dx * dx + dz * dz < r2) {
				const float speed_mult = glm::clamp(bike->speed / crack_speed_ref,
				                                    crack_speed_min_mult, crack_speed_max_mult);
				bike->crack_impulse  = cfg.strength * speed_mult;
				// cooldown_dist / speed keeps retrigger rate constant per metre travelled
				static constexpr float MIN_SPEED_FOR_COOLDOWN = 1.f; // m/s floor
				bike->crack_cooldown = cfg.cooldown_dist / glm::max(bike->speed, MIN_SPEED_FOR_COOLDOWN);
				break;
			}
		}
	}

	if (crack_debug_draw) {
		for (const auto& ci : crack_instances) {
			Debug::add_sphere(ci.pos + glm::vec3(0, 0.1f, 0),
			                  ci.trigger_radius,
			                  Color32(0xff, 0x44, 0x00, 0xcc), -1.f);
		}
	}
}

static void crack_debug_menu()
{
	ImGui::SeparatorText("Crack Triggers");
	ImGui::Checkbox("debug draw", &crack_debug_draw);
	ImGui::DragFloat("speed_ref",      &crack_speed_ref,      0.5f,  1.f, 40.f);
	ImGui::DragFloat("speed_min_mult", &crack_speed_min_mult, 0.01f, 0.f, 1.f);
	ImGui::DragFloat("speed_max_mult", &crack_speed_max_mult, 0.05f, 1.f, 5.f);
	for (int ti = 0; ti < NUM_CRACK_TYPES; ++ti) {
		CrackTypeConfig& cfg = crack_types[ti];
		ImGui::PushID(ti);
		char label[64];
		snprintf(label, sizeof(label), "Type %d (%d found): %s", ti, cfg.count, cfg.material_path);
		if (ImGui::CollapsingHeader(label)) {
			ImGui::DragFloat("strength",    &cfg.strength,    0.05f, 0.f,  5.f);
			ImGui::DragFloat("radius_mult", &cfg.radius_mult, 0.05f, 0.1f, 5.f);
			ImGui::DragFloat("cooldown_dist (m)", &cfg.cooldown_dist, 0.1f, 0.1f, 20.f);
			ImGui::TextDisabled("(radius_mult scales each decal's natural XZ footprint)");
		}
		ImGui::PopID();
	}
}
ADD_TO_DEBUG_MENU(crack_debug_menu);
