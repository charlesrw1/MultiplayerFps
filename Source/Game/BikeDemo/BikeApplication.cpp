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
#include <SDL2/SDL_gamecontroller.h>
#include <glm/gtc/matrix_transform.hpp>
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "UI/Gui.h"
#include <algorithm>
#include <fstream>
#include <random>

// Wind state is accessed via the global WindSystem: g_wind.wind_speed, etc.

// Steering expo: >1 compresses small deflections for finer control near center.
// At expo=2: half-stick → 25% input. At expo=1: linear. At expo=3: half-stick → 12.5%.
static float steer_expo = 1.25f;

// Speed hold tuning
static float sh_power_up   = 0.012f;
static float sh_power_down = 0.025f;
static float sh_power_max  = 800.f;

// Freewheel sound fade
static float free_wheel_fade = 0.0002f;

// Debug pointers (set each frame in evaluate; bp_for_debug is non-static so BikeCamera.cpp can extern it)
static BikeObject* bo_for_debug = nullptr;
BikePlayer*        bp_for_debug = nullptr;
static BikeGameApplication* g_bike_app   = nullptr;
BikeAIParams g_ai_params;

// Forward declare — defined later alongside the boid debug menu
static void apply_debug_follow_camera();
static bool g_follow_rider = false;

// ============================================================
// Helpers
// ============================================================

static float apply_deadzone(float val, float dz) {
	if (glm::abs(val) < dz) return 0.f;
	return glm::sign(val) * (glm::abs(val) - dz) / (1.f - dz);
}

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
extern CrackTypeConfig crack_types[NUM_CRACK_TYPES];  // defined near update_crack_triggers

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
	GameplayStatic::change_level("maps/bike_test_map.tmap");

	course.build_from_spawners();

	collect_crack_decals();

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

	// Spawn player at the course start
	const glm::vec3 start_pos = course.is_built
		? course.sample(0.f).position
		: glm::vec3(0.f);
	create_player(start_pos);

	// Spawn AI riders staggered 5 m apart behind the player
	static constexpr float AI_GAP_M      = 5.f;   // spacing along course (m)
	static constexpr float AI_LAT_SPREAD = 1.2f;  // lateral spread so they don't all overlap
	for (int i = 0; i < num_ai; ++i) {
		const float dist   = -(i + 1) * AI_GAP_M;  // behind player
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
	course.build_from_spawners();
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

// ============================================================
// AI Debug Recorder
// ============================================================

struct AIDebugFrame {
    float time_s;
    int   rider_idx;
    bool  is_ai;
    bool  is_crashed;
    bool  is_off_track;  // |lateral_pos| > road_half_width
    bool  is_colliding;  // nearest rider within 0.76 m lat and 2 m long

    // Kinematics
    float course_dist_m;
    float lateral_pos;
    float lateral_vel;
    float speed_ms;
    float heading_deg;   // atan2(dir.x, dir.z) in degrees from +Z

    // Course geometry at sample position
    float road_half_width;
    float rl_lateral;    // racing-line lateral offset (positive = road-right)
    float lat_err;       // lateral_pos - rl_lateral

    // Steering (AI only; 0 for player)
    float steer_near;
    float steer_far;
    float edge_steer;
    float avoid_steer;
    float pred_lateral;   // worst arc-predicted lateral (edge avoidance)
    float steer_pre_hard; // steer after avoidance, before hard yield clamp
    float hard_steer_min;
    float hard_steer_max;
    float steer_final;    // AI steer output
    float steer_committed;// inertia-smoothed steer from physics

    // Braking
    float brake_amount;
    float avoid_brake;
    float brake_dist_m;   // distance to braking corner
    float v_max_corner;   // safe speed for that corner
    float brake_corner_r;
    float min_r;          // tightest radius in scan window

    // Power
    float power_base;
    float power_final;
    float actual_power;
    float boid_long_sep_power;

    // Pack context
    float avoidance_sep_steer;
    float avoidance_brake;

    // Nearest rider (longitudinally closest within 10 m)
    float nearest_lat_sep_m;
    float nearest_long_gap_m;

    // Lookahead point fed to the steer PD (BikeAI::dbg_lookahead_pt)
    float lookahead_pt_x, lookahead_pt_y, lookahead_pt_z;
    float lookahead_dist;     // distance from rider to lookahead pt
    int   has_wheel;          // 1 = follower (wheel-anchored target), 0 = leader (racing-line)
    int   wheel_idx;          // index of wheel rider, or -1 if none
    int   paceline_state;     // 0=Following 1=Pulling 2=Peeling 3=DriftingBack
    float paceline_timer_s;
    float lat_offset;         // smoothed
    float lat_offset_target;  // resolver pick this frame
    float lat_offset_bias;
    float corner_factor;
};

static constexpr int   AI_DBG_MAX_FRAMES = 100000;
static constexpr float AI_DBG_INTERVAL   = 0.05f;

static std::vector<AIDebugFrame> s_ai_dbg_frames;
static bool  s_ai_dbg_recording = false;
static float s_ai_dbg_accum     = 0.f;
static float s_ai_dbg_time      = 0.f;
static bool  s_ai_dbg_full      = false;

static void ai_debug_record(BikeGameApplication* app)
{
    if (!app->course.is_built) return;
    const auto& all = app->all_riders;
    const int n = (int)all.size();

    for (int i = 0; i < n; ++i) {
        if ((int)s_ai_dbg_frames.size() >= AI_DBG_MAX_FRAMES) {
            s_ai_dbg_full = true;
            s_ai_dbg_recording = false;
            return;
        }

        const BikeObject* bo = all[i];
        const BikeAI*     ai = dynamic_cast<const BikeAI*>(bo->input.get());

        AIDebugFrame fr{};
        fr.time_s    = s_ai_dbg_time;
        fr.rider_idx = i;
        fr.is_ai     = (ai != nullptr);
        fr.is_crashed = bo->is_crashed;

        fr.course_dist_m = bo->course_dist_m;
        fr.lateral_pos   = bo->lateral_pos;
        fr.lateral_vel   = bo->lateral_vel;
        fr.speed_ms      = bo->speed;
        fr.heading_deg   = glm::degrees(glm::atan(bo->bike_direction.x, bo->bike_direction.z));

        fr.road_half_width = app->course.get_road_half_width(bo->course_segment);
        fr.is_off_track    = glm::abs(bo->lateral_pos) > fr.road_half_width;

        const BikeWaypoint wp = app->course.sample(bo->course_dist_m);
        fr.rl_lateral = glm::dot(wp.racing_line_pos - wp.position, wp.right);
        fr.lat_err    = bo->lateral_pos - fr.rl_lateral;

        fr.nearest_lat_sep_m  = 999.f;
        fr.nearest_long_gap_m = 999.f;
        for (int j = 0; j < n; ++j) {
            if (j == i) continue;
            const float long_gap = glm::abs(all[j]->course_dist_m - bo->course_dist_m);
            if (long_gap > 10.f) continue;
            const float lat_sep = glm::abs(all[j]->lateral_pos - bo->lateral_pos);
            if (lat_sep < fr.nearest_lat_sep_m) {
                fr.nearest_lat_sep_m  = lat_sep;
                fr.nearest_long_gap_m = long_gap;
            }
        }
        fr.is_colliding = (fr.nearest_lat_sep_m < 0.76f && fr.nearest_long_gap_m < 2.f);

        fr.avoidance_sep_steer = bo->avoidance_sep_steer;
        fr.avoidance_brake     = bo->avoidance_brake;
        fr.boid_long_sep_power = bo->boid_long_sep_power;
        fr.steer_committed     = bo->steer_committed;
        fr.actual_power        = bo->stamina.actual_power;

        if (ai) {
            fr.steer_near        = ai->dbg_steer_near;
            fr.steer_far         = ai->dbg_steer_far;
            fr.edge_steer        = ai->dbg_edge_steer;
            fr.avoid_steer       = ai->dbg_avoid_steer;
            fr.pred_lateral      = ai->dbg_pred_lateral;
            fr.steer_pre_hard    = ai->dbg_steer_pre_hard;
            fr.hard_steer_min    = ai->hard_steer_min;
            fr.hard_steer_max    = ai->hard_steer_max;
            fr.steer_final       = ai->dbg_steer_final;
            fr.brake_amount      = ai->dbg_brake_amount;
            fr.avoid_brake       = ai->dbg_avoid_brake;
            fr.brake_dist_m      = ai->dbg_brake_dist_m;
            fr.v_max_corner      = ai->dbg_v_max;
            fr.brake_corner_r    = ai->dbg_brake_corner_r;
            fr.min_r             = ai->dbg_min_r;
            fr.power_base        = ai->dbg_power_base;
            fr.power_final       = ai->dbg_power_final;
            fr.lookahead_pt_x    = ai->dbg_lookahead_pt.x;
            fr.lookahead_pt_y    = ai->dbg_lookahead_pt.y;
            fr.lookahead_pt_z    = ai->dbg_lookahead_pt.z;
            fr.lookahead_dist    = ai->dbg_lookahead_dist;
            fr.has_wheel         = ai->dbg_has_wheel ? 1 : 0;
            fr.wheel_idx         = -1;
            if (ai->wheel) {
                for (int wj = 0; wj < n; ++wj) if (all[wj] == ai->wheel) { fr.wheel_idx = wj; break; }
            }
            fr.paceline_state    = (int)ai->paceline_state;
            fr.paceline_timer_s  = ai->paceline_timer_s;
            fr.lat_offset        = ai->lat_offset;
            fr.lat_offset_target = ai->dbg_lat_offset_target;
            fr.lat_offset_bias   = ai->lat_offset_bias;
            fr.corner_factor     = ai->dbg_corner_factor;
        } else {
            fr.wheel_idx = -1;
        }

        s_ai_dbg_frames.push_back(fr);
    }
}

static void ai_debug_dump(BikeGameApplication* app)
{
    {
        std::ofstream f("D:/Data/ai_debug_dump.csv");
        if (!f.is_open()) {
            sys_print(Warning, "AI debug recorder: failed to open D:/Data/ai_debug_dump.csv\n");
            return;
        }
        f << "time_s,rider_idx,is_ai,is_crashed,is_off_track,is_colliding,"
             "course_dist_m,lateral_pos,lateral_vel,speed_ms,heading_deg,"
             "road_half_width,rl_lateral,lat_err,"
             "steer_near,steer_far,edge_steer,avoid_steer,pred_lateral,"
             "steer_pre_hard,hard_steer_min,hard_steer_max,steer_final,steer_committed,"
             "brake_amount,avoid_brake,brake_dist_m,v_max_corner,brake_corner_r,min_r,"
             "power_base,power_final,actual_power,boid_long_sep_power,"
             "avoidance_sep_steer,avoidance_brake,"
             "nearest_lat_sep_m,nearest_long_gap_m,"
             "lookahead_pt_x,lookahead_pt_y,lookahead_pt_z,lookahead_dist,has_wheel,"
             "wheel_idx,paceline_state,paceline_timer_s,lat_offset,lat_offset_target,lat_offset_bias,corner_factor\n";
        for (const auto& fr : s_ai_dbg_frames) {
            f << fr.time_s              << ','
              << fr.rider_idx           << ',' << (int)fr.is_ai         << ','
              << (int)fr.is_crashed     << ',' << (int)fr.is_off_track  << ',' << (int)fr.is_colliding << ','
              << fr.course_dist_m       << ',' << fr.lateral_pos        << ',' << fr.lateral_vel      << ','
              << fr.speed_ms            << ',' << fr.heading_deg        << ','
              << fr.road_half_width     << ',' << fr.rl_lateral         << ',' << fr.lat_err           << ','
              << fr.steer_near          << ',' << fr.steer_far          << ',' << fr.edge_steer        << ','
              << fr.avoid_steer         << ',' << fr.pred_lateral       << ','
              << fr.steer_pre_hard      << ',' << fr.hard_steer_min     << ',' << fr.hard_steer_max    << ','
              << fr.steer_final         << ',' << fr.steer_committed    << ','
              << fr.brake_amount        << ',' << fr.avoid_brake        << ',' << fr.brake_dist_m      << ','
              << fr.v_max_corner        << ',' << fr.brake_corner_r     << ',' << fr.min_r             << ','
              << fr.power_base          << ',' << fr.power_final        << ',' << fr.actual_power      << ','
              << fr.boid_long_sep_power << ','
              << fr.avoidance_sep_steer << ',' << fr.avoidance_brake    << ','
              << fr.nearest_lat_sep_m   << ',' << fr.nearest_long_gap_m << ','
              << fr.lookahead_pt_x      << ',' << fr.lookahead_pt_y     << ',' << fr.lookahead_pt_z << ','
              << fr.lookahead_dist      << ',' << fr.has_wheel          << ','
              << fr.wheel_idx           << ',' << fr.paceline_state     << ',' << fr.paceline_timer_s << ','
              << fr.lat_offset          << ',' << fr.lat_offset_target  << ',' << fr.lat_offset_bias  << ','
              << fr.corner_factor       << '\n';
        }
        sys_print(Debug, "AI debug recorder: wrote %d frames to D:/Data/ai_debug_dump.csv\n",
                  (int)s_ai_dbg_frames.size());
    }
    if (!app || !app->course.is_built) return;
    {
        std::ofstream cf("D:/Data/ai_debug_course.csv");
        if (!cf.is_open()) return;
        cf << "wp_idx,pos_x,pos_y,pos_z,fwd_x,fwd_z,right_x,right_z,"
              "rl_pos_x,rl_pos_y,rl_pos_z,rl_lateral,road_hw\n";
        const int nwp = (int)app->course.waypoints.size();
        for (int i = 0; i < nwp; ++i) {
            const BikeWaypoint& cwp = app->course.waypoints[i];
            const float rhw    = app->course.get_road_half_width(i);
            const float rl_lat = glm::dot(cwp.racing_line_pos - cwp.position, cwp.right);
            cf << i                      << ','
               << cwp.position.x         << ',' << cwp.position.y         << ',' << cwp.position.z << ','
               << cwp.forward.x          << ',' << cwp.forward.z          << ','
               << cwp.right.x            << ',' << cwp.right.z            << ','
               << cwp.racing_line_pos.x  << ',' << cwp.racing_line_pos.y  << ',' << cwp.racing_line_pos.z << ','
               << rl_lat << ',' << rhw << '\n';
        }
        sys_print(Debug, "AI debug recorder: wrote %d waypoints to D:/Data/ai_debug_course.csv\n", nwp);
    }
}

void BikeGameApplication::update()
{
	GameplayStatic::reset_debug_text_height();

	g_bike_app = this;
	if (course.is_built) {
		update_course_positions();
		sort_riders();
		update_groups();
		update_drafting();
		update_wheel_picking();
		update_paceline();
		update_clear_air();
		update_avoidance();
		update_gap_regulation();

	}
	update_crack_triggers();
	apply_debug_follow_camera();

	if (s_ai_dbg_recording && !s_ai_dbg_full) {
		const float dt = eng->get_dt();
		s_ai_dbg_accum += dt;
		s_ai_dbg_time  += dt;
		if (s_ai_dbg_accum >= AI_DBG_INTERVAL) {
			ai_debug_record(this);
			s_ai_dbg_accum -= AI_DBG_INTERVAL;
		}
	}
}

void BikeGameApplication::update_course_positions()
{
	for (auto* r : all_riders) {
		r->course_dist_m = course.project(
			r->get_ws_position(),
			&r->lateral_pos,
			&r->course_segment,
			r->course_dist_m);   // prev_dist_m: restricts search to ±window, prevents loop aliasing
	}
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

// ============================================================
// Group context
// ============================================================

static constexpr float GROUP_GAP_M = 8.f;  // gap > this splits riders into separate groups
static bool show_bike_groups = true;
void BikeGameApplication::update_groups()
{
	const int n = (int)riders_sorted.size();
	if (n == 0) return;

	// Scan riders front-to-back; start a new group when the gap to the next rider exceeds GROUP_GAP_M.
	// riders_sorted[0] = race leader (highest course_dist_m).
	int group_id    = 0;
	int group_start = 0;

	// Temporary arrays to avoid two passes
	std::vector<int> gids(n);
	std::vector<int> gsizes;

	gids[0] = 0;
	for (int i = 1; i < n; ++i) {
		const float gap = riders_sorted[i - 1]->course_dist_m - riders_sorted[i]->course_dist_m;
		if (gap > GROUP_GAP_M) {
			gsizes.push_back(i - group_start);
			++group_id;
			group_start = i;
		}
		gids[i] = group_id;
	}
	gsizes.push_back(n - group_start);
	const int num_groups = group_id + 1;

	if (show_bike_groups) {
		for (int i = 0; i < num_groups; i++) {
			Bounds b;
			bool found_bounds = false;
			for (int r = 0; r < n; r++) {
				if (gids.at(r) == i) {
					Bounds rider_bounds = Bounds(riders_sorted.at(r)->prev_front_wheel_pos, riders_sorted.at(r)->prev_rear_wheel_pos);
					if (!found_bounds)
						b = Bounds(rider_bounds);
					else
						b = bounds_union(b, rider_bounds);
					found_bounds = true;
				}
			}
			if (found_bounds)
				Debug::add_box(b.get_center(), (b.bmax - b.bmin), COLOR_PINK, -1);
		}
	}

	int pos_in_group = 0;
	for (int i = 0; i < n; ++i) {
		if (i > 0 && gids[i] != gids[i - 1])
			pos_in_group = 0;

		BikeObject* b = riders_sorted[i];
		b->group_id          = gids[i];
		const int gsz        = gsizes[gids[i]];
		b->pos_in_group_norm = (gsz > 1) ? (float)pos_in_group / (float)(gsz - 1) : 0.f;
		b->group_rank_norm   = (num_groups > 1) ? (float)gids[i] / (float)(num_groups - 1) : 0.f;
		b->group_size_norm   = (float)gsz / (float)n;
		++pos_in_group;
	}
}

// ============================================================
// Drafting constants
// ============================================================
static constexpr float DRAFT_LONG_MIN    =  0.3f;  // min longitudinal gap to benefit (m)
static constexpr float DRAFT_LONG_MAX    =  8.0f;  // no benefit beyond this (m)
static constexpr float DRAFT_LAT_MAX     =  1.2f;  // no benefit beyond this lateral offset (m)
static constexpr float DRAFT_MAX_BENEFIT =  0.35f; // max CdA reduction (35%)
static constexpr float DRAFT_FLOOR       =  0.55f; // minimum draft_factor (hard floor)
static constexpr int   DRAFT_STACK_CHECK =  5;     // how many riders ahead to check for stacking

void BikeGameApplication::update_drafting()
{
	const int n = (int)riders_sorted.size();

	for (int i = 0; i < n; ++i) {
		BikeObject* me = riders_sorted[i];

		float total_benefit  = 0.f;
		float stack_weight   = 1.f;

		// riders_sorted is front-to-back (index 0 = leader), so riders ahead are at lower indices
		for (int j = i - 1; j >= 0 && (i - j) <= DRAFT_STACK_CHECK; --j) {
			const BikeObject* ahead = riders_sorted[j];

			const float long_gap = ahead->course_dist_m - me->course_dist_m;
			const float lat_gap  = glm::abs(ahead->lateral_pos - me->lateral_pos);

			if (long_gap < DRAFT_LONG_MIN || long_gap > DRAFT_LONG_MAX) continue;
			if (lat_gap  >= DRAFT_LAT_MAX)                               continue;

			// Benefit falls off linearly with both gaps
			const float long_factor = 1.f - (long_gap - DRAFT_LONG_MIN) / (DRAFT_LONG_MAX - DRAFT_LONG_MIN);
			const float lat_factor  = 1.f - lat_gap / DRAFT_LAT_MAX;
			const float benefit     = long_factor * lat_factor * DRAFT_MAX_BENEFIT;

			total_benefit += benefit * stack_weight;
			stack_weight  *= 0.5f;  // each additional rider in the stack contributes half as much
		}

		me->draft_factor = glm::max(DRAFT_FLOOR, 1.f - total_benefit);
	}
}

// ============================================================
// Pack / gap constants
// ============================================================

// Longitudinal power yield when side-by-side: the slightly-behind rider sheds watts
static float SIDE_BY_SIDE_LONG_M   = 2.5f;   // longitudinal range to count as abreast (m)
static float SIDE_BY_SIDE_LAT_M    = 1.0f;   // lateral range to count as abreast (m)
static float SIDE_BY_SIDE_POWER_W  = 60.f;   // max W shed / gained

// Predictive inter-rider avoidance (soft steer push, truly side-by-side only)
// Only fires when riders are nearly abreast (long_gap < AVOID_LONG_MAX).
// Weighted by (1 - long_gap / AVOID_LONG_MAX) so the push fades longitudinally.
static float AVOID_LONG_MAX     = 3.5f;   // m — longitudinal range; beyond this = single file, no push
static float AVOID_BIKE_HALF_W  = 0.38f;  // m — half shoulder width
static float AVOID_CLEARANCE    = 0.25f;  // m — additional margin beyond bike width
static float AVOID_PREDICT_T1   = 0.5f;   // s — first prediction horizon
static float AVOID_PREDICT_T2   = 1.0f;   // s — second prediction horizon
static float AVOID_STEER_KP     = 0.8f;   // additive steer per m of predicted overlap (soft nudge)

// Priority yield (hard enforcement — lower-priority yielder only)
// Lower index in riders_sorted = further ahead = higher priority.
// The yielder (higher index) is forbidden from steering into the higher-priority rider's zone.
// Sign convention fix vs. old HARD_SEP: positive steer = road-LEFT, negative = road-RIGHT.
//   Other road-right → block road-right movement → block negative steer → hard_steer_min = 0
//   Other road-left  → block road-left movement  → block positive steer → hard_steer_max = 0
static float YIELD_LONG_RADIUS  = 3.5f;   // m — longitudinal range for yield clamp
static float YIELD_OUTER_LAT    = 1.3f;   // m — engage clamp inside this lateral gap
static float YIELD_INNER_LAT    = 0.05f;  // m — disengage when already overlapping (escape)
static float YIELD_SQUEEZE_M    = 0.35f;  // m — available road width below this triggers brake
static float YIELD_BRAKE_K      = 0.55f;  // brake fraction when fully squeezed

// ============================================================
// Wheel picking — choose the rider directly ahead I'm following.
// Score candidates in (group, ahead by [long_min, long_max], lateral overlap) and
// pick the highest. Sets BikeAI::wheel each frame; null = leader.
// See [[bike/bikeai#Wheel picking]].
// ============================================================
void BikeGameApplication::update_wheel_picking()
{
	const BikeAIParams& p = g_ai_params;
	const int n = (int)riders_sorted.size();

	for (int i = 0; i < n; ++i) {
		BikeObject* me = riders_sorted[i];
		BikeAI* ai = dynamic_cast<BikeAI*>(me->input.get());
		if (!ai) continue;

		BikeObject* best       = nullptr;
		float       best_score = -FLT_MAX;
		const float long_norm  = glm::max(0.5f, p.wheel_long_max - p.wheel_long_min);

		for (int j = 0; j < n; ++j) {
			if (j == i) continue;
			BikeObject* other = riders_sorted[j];
			if (other->group_id != me->group_id) continue;

			// Skip riders in transient states — Peeling drifts sideways, DriftingBack
			// is a slow rider exiting the rotation. Following them just yanks the chain
			// off the line until they re-enter Following.
			if (BikeAI* oai = dynamic_cast<BikeAI*>(other->input.get())) {
				if (oai->paceline_state == PacelineState::Peeling ||
				    oai->paceline_state == PacelineState::DriftingBack) continue;
			}

			const float signed_long = other->course_dist_m - me->course_dist_m;
			if (signed_long < p.wheel_long_min || signed_long > p.wheel_long_max) continue;

			const float lat_gap = glm::abs(other->lateral_pos - (me->lateral_pos + ai->lat_offset));
			if (lat_gap > p.wheel_lat_max) continue;

			// Score components in [0,1]
			const float long_close = 1.f - glm::clamp(glm::abs(signed_long - p.wheel_long_gap)
			                                           / long_norm, 0.f, 1.f);
			const float lat_align  = 1.f - lat_gap / p.wheel_lat_max;
			const float draft_b    = 1.f - other->draft_factor;  // 0 = open air, ~0.45 = full draft

			float score = p.wheel_w_long  * long_close
			            + p.wheel_w_lat   * lat_align
			            + p.wheel_w_draft * draft_b;
			if (other == ai->wheel) score += p.wheel_stickiness;

			if (score > best_score) { best_score = score; best = other; }
		}

		if (best && best_score >= p.wheel_score_thresh) {
			ai->wheel = best;
		} else {
			ai->wheel = nullptr;
		}
		ai->dbg_has_wheel   = (ai->wheel != nullptr);
		ai->dbg_wheel_score = (best ? best_score : 0.f);
	}
}

// ============================================================
// Paceline tactical FSM — Following / Pulling / Peeling / DriftingBack.
//
// Wheel-picker is the "what wheel am I on" decision. This is "what am I doing
// strategically" — it modifies the wheel result (forces a peel-side lat_offset,
// scales target_power) and gates pull re-entry.
//
// Cascade-safe promotion: Pulling riders accelerate, so the gap to the next
// rider can exceed wheel_long_max within seconds. is_at_front() walks ALL
// riders ahead in the same group with no distance cutoff; only Peeling and
// DriftingBack are skipped (transient sideways/slow states). Without this,
// every following rider eventually self-promotes when its puller pulls away.
// See [[bike/bikeai#Tactical FSM]].
// ============================================================
void BikeGameApplication::update_paceline()
{
	const BikeAIParams& p = g_ai_params;
	const float dt = eng->get_dt();
	const int   n  = (int)riders_sorted.size();

	auto is_at_front = [&](int idx) -> bool {
		BikeObject* me = riders_sorted[idx];
		for (int j = idx - 1; j >= 0; --j) {
			BikeObject* other = riders_sorted[j];
			if (other->group_id != me->group_id) continue;
			BikeAI* oai = dynamic_cast<BikeAI*>(other->input.get());
			if (!oai) return false;  // player counts as a stable wheel
			if (oai->paceline_state == PacelineState::Peeling ||
			    oai->paceline_state == PacelineState::DriftingBack) continue;
			return false;
		}
		return true;
	};

	for (int i = 0; i < n; ++i) {
		BikeObject* me = riders_sorted[i];
		BikeAI*     ai = dynamic_cast<BikeAI*>(me->input.get());
		if (!ai) continue;

		ai->paceline_timer_s += dt;
		if (ai->pull_cooldown_s > 0.f)
			ai->pull_cooldown_s = glm::max(0.f, ai->pull_cooldown_s - dt);

		const bool at_front = is_at_front(i);

		switch (ai->paceline_state) {
		case PacelineState::Following:
			// Becoming a leader by emergence triggers a pull (unless still cooling down).
			if (at_front && ai->pull_cooldown_s <= 0.f) {
				ai->paceline_state   = PacelineState::Pulling;
				ai->paceline_timer_s = 0.f;
			}
			break;

		case PacelineState::Pulling:
			// Pull until duration elapses, then peel off. If a stable rider appears
			// ahead (someone moved up), drop back to Following without peeling.
			if (!at_front) {
				ai->paceline_state   = PacelineState::Following;
				ai->paceline_timer_s = 0.f;
			} else if (ai->paceline_timer_s >= p.pull_duration_s) {
				ai->paceline_state   = PacelineState::Peeling;
				ai->paceline_timer_s = 0.f;
				// Peel AWAY from road centre (right if exactly centred). Matches
				// the BikeAIPaceline.PeelDir tests.
				ai->peel_side_sign = (me->lateral_pos < 0.f) ? -1.f : +1.f;
			}
			break;

		case PacelineState::Peeling:
			if (ai->paceline_timer_s >= p.peel_duration_s) {
				ai->paceline_state   = PacelineState::DriftingBack;
				ai->paceline_timer_s = 0.f;
			}
			break;

		case PacelineState::DriftingBack:
			// Done drifting once we've found a stable wheel, or after a hard cap.
			if (ai->wheel || ai->paceline_timer_s >= p.drift_duration_s) {
				ai->paceline_state   = PacelineState::Following;
				ai->paceline_timer_s = 0.f;
				ai->pull_cooldown_s  = p.pull_cooldown_s;
			}
			break;
		}
	}
}

// ============================================================
// Clear-air resolver — pick lat_offset from open slots around the wheel.
// Default ideal slot = 0 (best draft) + personality bias. If neighbors block
// it within [clear_air_long_window × clear_air_lat_window], shift to the
// candidate offset with the most open air. Smoothed over clear_air_damp_tau.
// Peeling state forces lat_offset toward ±peel_offset_m, bypassing the search.
// See [[bike/bikeai#Lateral offset rule]].
// ============================================================
void BikeGameApplication::update_clear_air()
{
	const BikeAIParams& p = g_ai_params;
	const float dt = eng->get_dt();
	const int   n  = (int)riders_sorted.size();
	// Damping coefficient — frame-rate independent low-pass.
	const float lerp = (p.clear_air_damp_tau > 1e-3f)
	                   ? (1.f - glm::exp(-dt / p.clear_air_damp_tau))
	                   : 1.f;

	for (int i = 0; i < n; ++i) {
		BikeObject* me = riders_sorted[i];
		BikeAI*     ai = dynamic_cast<BikeAI*>(me->input.get());
		if (!ai) continue;

		// Peeling: forced offset, no search.
		if (ai->paceline_state == PacelineState::Peeling) {
			const float target = ai->peel_side_sign * p.peel_offset_m;
			ai->dbg_lat_offset_target = target;
			ai->lat_offset += (target - ai->lat_offset) * lerp;
			continue;
		}

		// Leader: lat_offset is unused (racing line is the target). Decay toward 0
		// so re-acquiring a wheel starts from the bias-neutral position.
		if (!ai->wheel) {
			ai->dbg_lat_offset_target = ai->lat_offset_bias;
			ai->lat_offset += (ai->lat_offset_bias - ai->lat_offset) * lerp;
			continue;
		}

		// Follower: search candidate offsets in the wheel's road frame.
		const BikeWaypoint wheel_wp    = course.sample(ai->wheel->course_dist_m);
		const glm::vec3    wheel_pos   = ai->wheel->get_ws_position();
		const glm::vec3    wheel_fwd   = ai->wheel->bike_direction;
		const glm::vec3    wheel_right = wheel_wp.right;
		const float        road_hw     = course.get_road_half_width(ai->wheel->course_segment);
		const float        safe_lat    = road_hw - p.edge_safety_m;
		const float        bias        = ai->lat_offset_bias;

		const int   half_steps  = (int)glm::ceil(p.clear_air_max_offset / glm::max(p.clear_air_step, 0.05f));
		const float lat_inv     = 1.f / glm::max(p.clear_air_lat_window,  1e-3f);
		const float long_inv    = 1.f / glm::max(p.clear_air_long_window, 1e-3f);

		float best_score  = FLT_MAX;
		float best_offset = bias;

		for (int s = -half_steps; s <= half_steps; ++s) {
			const float cand = bias + (float)s * p.clear_air_step;
			const float cand_world_lat = ai->wheel->lateral_pos + cand;
			if (glm::abs(cand_world_lat) > safe_lat) continue;

			const glm::vec3 cand_pos = wheel_pos
			                         - wheel_fwd   * ai->long_gap
			                         + wheel_right * cand;

			float occ = 0.f;
			for (int j = 0; j < n; ++j) {
				if (j == i) continue;
				BikeObject* other = riders_sorted[j];
				if (other == ai->wheel) continue;
				if (other->group_id != me->group_id) continue;

				const glm::vec3 d    = other->get_ws_position() - cand_pos;
				const float dl   = glm::dot(d, wheel_fwd);
				const float dlat = glm::dot(d, wheel_right);
				if (glm::abs(dl)   > p.clear_air_long_window) continue;
				if (glm::abs(dlat) > p.clear_air_lat_window)  continue;
				const float lat_close  = 1.f - glm::abs(dlat) * lat_inv;
				const float long_close = 1.f - glm::abs(dl)   * long_inv;
				occ += lat_close * long_close;
			}
			const float score = occ + p.clear_air_center_bias * glm::abs(cand - bias);
			if (score < best_score) { best_score = score; best_offset = cand; }
		}

		ai->dbg_lat_offset_target = best_offset;
		ai->lat_offset += (best_offset - ai->lat_offset) * lerp;

		// Final road clamp on the smoothed value (the resolver already filtered
		// out off-road candidates, but a small overshoot can still slip through
		// because it tracks the wheel's prior frame).
		const float lat_world = ai->wheel->lateral_pos + ai->lat_offset;
		if (glm::abs(lat_world) > safe_lat)
			ai->lat_offset = glm::sign(lat_world) * safe_lat - ai->wheel->lateral_pos;
	}
}

// ============================================================
// Predictive lateral avoidance + priority-yield clamp + side-by-side power yield.
// Reflex layer: catches surges, brake events, line changes the clear-air resolver
// (which acts on a longer 1.5s timescale) cannot react to in time.
// See [[bike/bikeai#Rider-rider avoidance]].
// ============================================================
void BikeGameApplication::update_avoidance()
{
	const int   n  = (int)riders_sorted.size();
	const float dt = eng->get_dt();

	for (int i = 0; i < n; ++i) {
		BikeObject* me = riders_sorted[i];

		// Longitudinal power yield: when side-by-side, the slightly-behind rider sheds watts.
		float long_sep_power = 0.f;
		for (int j = 0; j < n; ++j) {
			if (j == i) continue;
			const BikeObject* other = riders_sorted[j];
			const float signed_gap = other->course_dist_m - me->course_dist_m;
			const float long_gap   = glm::abs(signed_gap);
			const float lat_gap    = glm::abs(me->lateral_pos - other->lateral_pos);
			if (long_gap >= SIDE_BY_SIDE_LONG_M || lat_gap >= SIDE_BY_SIDE_LAT_M) continue;
			const float long_weight = 1.f - long_gap / SIDE_BY_SIDE_LONG_M;
			long_sep_power += (signed_gap > 0.f ? 1.f : -1.f) * long_weight * SIDE_BY_SIDE_POWER_W;
		}
		me->boid_long_sep_power = glm::clamp(long_sep_power, -SIDE_BY_SIDE_POWER_W, SIDE_BY_SIDE_POWER_W);

		// ---- Predictive lateral separation (soft push, side-by-side only) ----
		// Only applies when riders are nearly abreast (long_gap < AVOID_LONG_MAX).
		// Weighted by (1 - long_gap/AVOID_LONG_MAX) so single-file trains get zero push.
		// BikeAI reads avoidance_sep_steer and adds it after edge avoidance.
		{
			BikeAI* me_ai = dynamic_cast<BikeAI*>(me->input.get());
			float avoid_accum = 0.f;
			const float full_sep = (AVOID_BIKE_HALF_W + AVOID_CLEARANCE) * 2.f;

			for (int j = 0; j < n; ++j) {
				if (j == i) continue;
				const BikeObject* other = riders_sorted[j];
				// Skip my wheel: drafting it intentionally; clear-air resolver handles offset.
				if (me_ai && other == me_ai->wheel) continue;
				const float long_gap = glm::abs(other->course_dist_m - me->course_dist_m);
				if (long_gap >= AVOID_LONG_MAX) continue;
				const float long_weight = 1.f - long_gap / AVOID_LONG_MAX;

				float worst_overlap = 0.f;
				const float ts[3] = { 0.f, AVOID_PREDICT_T1, AVOID_PREDICT_T2 };
				for (float t : ts) {
					const float me_lat  = me->lateral_pos    + me->lateral_vel    * t;
					const float o_lat   = other->lateral_pos + other->lateral_vel * t;
					const float overlap = glm::max(0.f, full_sep - glm::abs(me_lat - o_lat));
					worst_overlap = glm::max(worst_overlap, overlap);
				}

				if (worst_overlap > 0.f) {
					// dir_away: steer sign that moves me away from other.
					// I'm road-right → steer road-right (negative) → dir_away = -1
					// I'm road-left  → steer road-left  (positive) → dir_away = +1
					const float dir_away = (me->lateral_pos >= other->lateral_pos) ? -1.f : 1.f;
					avoid_accum += dir_away * long_weight * worst_overlap * AVOID_STEER_KP;
				}
			}
			float avoid_out = glm::clamp(avoid_accum, -1.f, 1.f);
			// Don't let avoidance push a rider further into the edge danger zone.
			// If the rider is already in the safety margin, suppress the component
			// that would move them closer to their near edge.
			{
				const float road_hw   = course.get_road_half_width(me->course_segment);
				const float safe_edge = road_hw - g_ai_params.edge_safety_m;
				if (glm::abs(me->lateral_pos) > safe_edge) {
					// near_edge_dir: +1 if near road-right edge, -1 if near road-left
					const float near_edge_dir = glm::sign(me->lateral_pos);
					// steer that moves toward the near edge is negative (road-right = steer negative)
					// i.e., steer_toward_edge = -near_edge_dir
					// suppress if avoid_out has same sign as steer_toward_edge direction
					if (glm::sign(avoid_out) == -near_edge_dir)
						avoid_out = 0.f;
				}
			}
			me->avoidance_sep_steer = avoid_out;
		}

		// ---- Priority yield: hard steer clamp + brake escape (BikeAI riders only) ----
		// Lower index in riders_sorted = further ahead in race = higher priority.
		// The yielder (higher index i) must not steer into a higher-priority rider's exclusion zone.
		// Sign: positive steer = road-LEFT (decreases lateral_pos).
		//   Other road-right of me → block road-right movement → block negative steer → min = 0
		//   Other road-left  of me → block road-left  movement → block positive steer → max = 0
		me->avoidance_brake = 0.f;
		BikeAI* ai_rider = dynamic_cast<BikeAI*>(me->input.get());
		if (ai_rider) {
			ai_rider->hard_steer_min = -1.f;
			ai_rider->hard_steer_max =  1.f;

			for (int j = 0; j < i; ++j) {  // only higher-priority riders
				const BikeObject* other = riders_sorted[j];
				// Skip my wheel: I'm intentionally drafting it. The clamp would otherwise
				// prevent me from converging onto the wheel's lateral track.
				if (other == ai_rider->wheel) continue;
				const float long_gap = glm::abs(other->course_dist_m - me->course_dist_m);
				if (long_gap >= YIELD_LONG_RADIUS) continue;
				const float lat_diff = other->lateral_pos - me->lateral_pos;  // +ve: other road-right
				const float h_lat    = glm::abs(lat_diff);
				if (h_lat >= YIELD_OUTER_LAT) continue;
				if (h_lat <  YIELD_INNER_LAT) continue;  // escape zone: already overlapping

				// Only clamp if I'm actively closing toward the other rider.
				// closing_vel > 0 when my lateral_vel is moving toward them.
				const float closing_vel = glm::sign(lat_diff) * me->lateral_vel;
				if (closing_vel <= 0.f) continue;  // already moving away — no clamp needed

				if (lat_diff > 0.f)
					ai_rider->hard_steer_min = 0.f;  // block road-right (negative steer)
				else
					ai_rider->hard_steer_max = 0.f;  // block road-left (positive steer)
			}

			// Squeeze detection: if clamped in one direction and road edge is close on the other.
			// Brake to create longitudinal gap when lateral escape is unavailable.
			const float road_hw = course.get_road_half_width(me->course_segment);
			const float cur_lat = me->lateral_pos;
			if (ai_rider->hard_steer_min == 0.f && ai_rider->hard_steer_max == 0.f) {
				// Boxed in on both sides
				me->avoidance_brake = YIELD_BRAKE_K;
			} else if (ai_rider->hard_steer_min == 0.f) {
				// Can't go road-right — check if road-left space is tight
				const float left_space = cur_lat + road_hw;
				if (left_space < YIELD_SQUEEZE_M)
					me->avoidance_brake = (1.f - left_space / YIELD_SQUEEZE_M) * YIELD_BRAKE_K;
			} else if (ai_rider->hard_steer_max == 0.f) {
				// Can't go road-left — check if road-right space is tight
				const float right_space = road_hw - cur_lat;
				if (right_space < YIELD_SQUEEZE_M)
					me->avoidance_brake = (1.f - right_space / YIELD_SQUEEZE_M) * YIELD_BRAKE_K;
			}
		}

		// Save lateral position and compute velocity for observation
		me->lateral_vel      = (dt > 1e-6f) ? (me->lateral_pos - me->prev_lateral_pos) / dt : 0.f;
		me->prev_lateral_pos = me->lateral_pos;
	}
}


// ============================================================
// Gap regulation — match the explicit wheel rider's power, with P-control on
// gap error. wheel == null  →  leader, holds tactical free power.
// See [[bike/bikeai#Power]].
// ============================================================

static float GAP_POWER_K         = 50.f;   // W correction per metre of gap error
static float GAP_POWER_MAX_DELTA = 250.f;  // max ±W applied on top of wheel rider's power
static float GAP_FREE_POWER_W    = 250.f;  // power when leader (no wheel)

void BikeGameApplication::update_gap_regulation()
{
	const BikeAIParams& p = g_ai_params;
	for (BikeObject* me : all_riders) {
		BikeAI* ai = dynamic_cast<BikeAI*>(me->input.get());
		if (!ai) continue;

		// Base target: match wheel power + P on gap, or free power if leader.
		float base = GAP_FREE_POWER_W;
		if (ai->wheel) {
			const glm::vec3 to_wheel = ai->wheel->get_ws_position() - me->get_ws_position();
			const float gap_m      = glm::dot(to_wheel, me->bike_direction);
			const float gap_err    = gap_m - ai->long_gap;  // +ve = too far back
			const float correction = glm::clamp(gap_err * GAP_POWER_K,
			                                    -GAP_POWER_MAX_DELTA, GAP_POWER_MAX_DELTA);
			base = ai->wheel->stamina.actual_power + correction;
		}

		// Tactical FSM modifies the base.
		switch (ai->paceline_state) {
		case PacelineState::Pulling:      base *= p.pull_power_frac;          break;
		case PacelineState::Peeling:      base += p.peel_power_delta_w;       break;
		case PacelineState::DriftingBack: base *= p.drift_power_frac;         break;
		case PacelineState::Following:    /* unchanged */                     break;
		}

		ai->target_power_watts = glm::clamp(base, 50.f, 1000.f);
	}
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
	bo->input = std::make_unique<BikePlayer>();
	all_riders.push_back(bo);
	riders_sorted.push_back(bo);
	return bo;
}

BikeObject* BikeGameApplication::create_ai(glm::vec3 pos)
{
	// Deterministic seed so same spawn order produces same biases each run.
	// ±0.1m keeps even biased riders within ~half a draft cone of the wheel's exact track.
	static std::mt19937 s_bias_rng(0xC0FFEEu);
	static std::uniform_real_distribution<float> s_bias_dist(-0.1f, 0.1f);

	Entity* e = GameplayStatic::spawn_entity();
	e->set_ws_position(pos);
	auto bo = e->create_component<BikeObject>();

	{
		auto ai    = std::make_unique<BikeAI>();
		ai->course = &course;
		ai->lat_offset_bias = s_bias_dist(s_bias_rng);

		bo->input = std::move(ai);
	}

	all_riders.push_back(bo);
	riders_sorted.push_back(bo);
	return bo;
}

// ============================================================
// BikePlayer — constructor / destructor
// ============================================================

BikePlayer::BikePlayer()
{
	auto* cc_entity = GameplayStatic::spawn_entity();
	auto* cc_comp   = cc_entity->create_component<CameraComponent>();
	cc_comp->set_is_enabled(true);
	cc_comp->set_fov(65.f);
	assert(CameraComponent::get_scene_camera() == cc_comp);
	this->cc = cc_comp;

	freewheel_player = isound->register_sound_player();
	freewheel_player->asset          = SoundFile::load("sounds/free_wheel.wav");
	freewheel_player->looping        = true;
	freewheel_player->attenuate      = false;
	freewheel_player->spatialize     = false;
	freewheel_player->volume_multiply = 0.f;
	freewheel_player->set_play(true);

	wind_player = isound->register_sound_player();
	wind_player->asset           = SoundFile::load("sounds/wind_loop.wav");
	wind_player->looping         = true;
	wind_player->attenuate       = false;
	wind_player->spatialize      = false;
	wind_player->volume_multiply = 0.f;
	wind_player->set_play(true);

	pedal_player = isound->register_sound_player();
	pedal_player->asset           = SoundFile::load("sounds/bike_pedal.wav");
	pedal_player->looping         = true;
	pedal_player->attenuate       = false;
	pedal_player->spatialize      = false;
	pedal_player->volume_multiply = 0.f;
	pedal_player->set_play(true);

	heart_icon_tex = Texture::load("ui/heart_icon.png");

	g_wind.init();
}

BikePlayer::~BikePlayer()
{
	isound->remove_sound_player(freewheel_player);
	isound->remove_sound_player(wind_player);
	isound->remove_sound_player(pedal_player);
	g_wind.shutdown();
	// speedlines particle obj cleaned up by BikeSpeedlinesFx dtor
}

// ============================================================
// BikePlayer::evaluate
// ============================================================
#include "Debug.h"
void BikePlayer::evaluate(BikeObject* my_bike)
{
	const float dt = eng->get_dt();

	// --- Gamepad input ---
	const float steer        = apply_deadzone((float)Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX), 0.15f);
	const float brake_amount = apply_deadzone((float)Input::get_con_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT), 0.05f);

	const bool power_up_press   = Input::was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_UP);
	const bool power_down_press = Input::was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	const bool power_up_held    = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_DPAD_UP);
	const bool power_down_held  = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	const bool coast_btn      = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_B);
	const bool speed_hold_btn = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_X);

	// Keyboard fallback
	const bool kb_left       = Input::is_key_down(SDL_SCANCODE_A) || Input::is_key_down(SDL_SCANCODE_LEFT);
	const bool kb_right      = Input::is_key_down(SDL_SCANCODE_D) || Input::is_key_down(SDL_SCANCODE_RIGHT);
	const bool kb_brake      = Input::is_key_down(SDL_SCANCODE_SPACE);
	const bool kb_up_press   = Input::was_key_pressed(SDL_SCANCODE_UP);
	const bool kb_down_press = Input::was_key_pressed(SDL_SCANCODE_DOWN);
	const bool kb_up_held    = Input::is_key_down(SDL_SCANCODE_UP);
	const bool kb_down_held  = Input::is_key_down(SDL_SCANCODE_DOWN);
	const bool kb_coast      = Input::is_key_down(SDL_SCANCODE_C);
	const bool kb_speed_hold = Input::is_key_down(SDL_SCANCODE_V);

	// --- Power level stepping: tap = 1 step, hold = rapid repeat after delay ---
	constexpr float POWER_HOLD_DELAY    = 0.2f;  // seconds before repeat starts
	constexpr float POWER_REPEAT_RATE   = 0.04f; // seconds per repeat step

	auto step_power = [&](int dir) {
		power_level_idx = glm::clamp(power_level_idx + dir, 0, BIKE_NUM_POWER_LEVELS - 1);
	};

	if (power_up_press   || kb_up_press)   step_power(+1);
	if (power_down_press || kb_down_press) step_power(-1);

	const int held_dir = (power_up_held || kb_up_held) ? 1 : (power_down_held || kb_down_held) ? -1 : 0;
	if (held_dir != 0) {
		power_hold_timer += dt;
		if (power_hold_timer >= POWER_HOLD_DELAY) {
			power_repeat_timer += dt;
			while (power_repeat_timer >= POWER_REPEAT_RATE) {
				step_power(held_dir);
				power_repeat_timer -= POWER_REPEAT_RATE;
			}
		}
	} else {
		power_hold_timer   = 0.f;
		power_repeat_timer = 0.f;
	}
	is_coasting = coast_btn || kb_coast || brake_amount > 0.f || kb_brake;

	// --- Speed hold ---
	const bool want_speed_hold = (speed_hold_btn || kb_speed_hold) && !is_coasting;
	if (want_speed_hold && !speed_hold_active) {
		speed_hold_active = true;
		speed_hold_target = my_bike->speed;
		speed_hold_power  = (float)BIKE_POWER_LEVELS[power_level_idx];
	} else if (!want_speed_hold) {
		speed_hold_active = false;
	}

	// --- Combine steer ---
	float steer_combined = steer;
	if (kb_left)  steer_combined -= 1.f;
	if (kb_right) steer_combined += 1.f;
	steer_combined = glm::clamp(steer_combined, -1.f, 1.f);

	// Expo curve: compresses small deflections for finer gamepad control near centre.
	// Keyboard is already binary (±1) so skip it there.
	if (!kb_left && !kb_right && glm::abs(steer_combined) > 0.0001f)
		steer_combined = glm::sign(steer_combined) * glm::pow(glm::abs(steer_combined), steer_expo);

	steer_combined = glm::clamp(steer_combined, -1.f, 1.f);
	this->dbg_steer_final = steer_combined;



	// --- Fill ControlInput ---
	BikeObject::ControlInput ci;
	ci.steer        = steer_combined;
	ci.brake_amount = kb_brake ? 1.f : brake_amount;

	if (speed_hold_active) {
		const float v            = glm::max(my_bike->speed, 0.3f);
		const float eff_wind_spd = g_wind.wind_speed * (1.f + g_wind.wind_gust_factor * 1.5f);
		const glm::vec3 wdir_n   = glm::length(g_wind.wind_direction) > 0.001f
		                           ? glm::normalize(g_wind.wind_direction) : glm::vec3(0.f);
		const float wind_along   = glm::dot(wdir_n, my_bike->bike_direction) * eff_wind_spd;
		const float app_speed    = my_bike->speed - wind_along;
		const float drag         = 0.5f * 1.225f * 0.3f * app_speed * glm::abs(app_speed);
		const float rolling      = 0.004f * 83.f * 9.81f;
		const float slope        = 83.f * 9.81f * glm::sin(my_bike->terrain_gradient);
		const float ideal_power  = glm::max((drag + rolling + slope) * v, 0.f);

		const float toward = (ideal_power > speed_hold_power) ? sh_power_up : sh_power_down;
		speed_hold_power = damp_dt_independent(ideal_power, speed_hold_power, toward, dt);
		speed_hold_power = glm::clamp(speed_hold_power, 0.f, sh_power_max);

		speed_hold_power = glm::clamp(speed_hold_power, 0.f, sh_power_max);
		ci.power = speed_hold_power;
	} else {
		ci.power = is_coasting ? 0.f : (float)BIKE_POWER_LEVELS[power_level_idx];
	}

	current_power = ci.power;
	my_bike->update_tick(ci);

	// --- Wind ---
	g_wind.update(my_bike, cam.camera_pos);

	// --- Camera ---
	if (!g_follow_rider)
		update_camera(my_bike, ci.steer, ci.brake_amount);

	// --- Sound: freewheel ---
	const float fw_target = ci.is_coasting() ? 1.f : 0.f;
	freewheel_player->volume_multiply = damp_dt_independent(fw_target, freewheel_player->volume_multiply, free_wheel_fade, dt);
	freewheel_player->pitch_multiply  = 0.8f + my_bike->speed * 0.04f;
	freewheel_player->update();

	// --- Sound: pedalling ---
	// Audible when power is applied; pitch tracks cadence (rev/s → semitone-linear scale).
	// Cadence of 1.5 rev/s (target ~90 RPM) maps to pitch 1.0.
	{
		float pedal_vol = ci.is_coasting() ? 0.f : 1.f;
		pedal_vol *= glm::clamp(my_bike->cadence / 0.9f, 0.f, 1.f) * 0.35f;
		pedal_player->volume_multiply = damp_dt_independent(pedal_vol, pedal_player->volume_multiply, 0.03f, dt);
		pedal_player->pitch_multiply = map_range(my_bike->cadence * 90.f, 70.f, 110.f, 0.95, 1.05);
		pedal_player->update();
	}

	// --- Sound: wind ambient ---
	{
		const float eff_spd = my_bike->speed-my_bike->get_wind_along_bike();// wind_speed* (1.f + wind_gust_factor * gust_speed_amp);
		const float vol_target = glm::clamp(eff_spd / 10.f, 0.f, 1.f)*0.8 + 0.4;
		wind_player->volume_multiply = damp_dt_independent(vol_target, wind_player->volume_multiply, 0.04f, dt);
		wind_player->pitch_multiply  = 0.75f + vol_target * 0.5f;
		wind_player->spatialize = true;
		auto wind_dir = my_bike->get_wind_along_bike_vector();
		//Debug::add_line(my_bike->get_ws_position(), my_bike->get_ws_position() + wind_dir, COLOR_WHITE, -1);
		wind_player->spatial_pos = cc->get_ws_position() - wind_dir;

		wind_player->update();
	}

	// --- Sound: gear change one-shot ---
	if (my_bike->just_shifted) {
		static const SoundFile* gear_snd = SoundFile::load("sounds/gear_change.wav");
		isound->play_sound(gear_snd, 0.2, 1.2f, 0.f, 0.f, SndAtn::Linear, false, false, {});
	}

	draw_power_meter(ci.power, power_level_idx, is_coasting, speed_hold_active, speed_hold_power,
	                 my_bike->stamina.actual_power, my_bike->stamina.power_ceiling);
	draw_stamina_ui(my_bike->stamina, my_bike->rider);

	bp_for_debug = this;
	bo_for_debug = my_bike;
}

// ============================================================
// Debug menu: Bike Status
// ============================================================

static void bike_status_debug()
{
	if (!bp_for_debug) return;
	const int power_level_idx = bp_for_debug->power_level_idx;
	const bool is_coasting    = bp_for_debug->is_coasting;
	auto* my_bike = bo_for_debug;

	const float speed_kmh = my_bike->speed * 3.6f;
	ImGui::Text("Speed:   %.1f km/h", speed_kmh);
	if (bp_for_debug->speed_hold_active)
		ImGui::Text("Power:   (speed hold %.1f km/h)  %.0f W",
			bp_for_debug->speed_hold_target * 3.6f, bp_for_debug->speed_hold_power);
	else
		ImGui::Text("Power:   %s  %d W",
			is_coasting ? "(coast)" : "",
			is_coasting ? 0 : BIKE_POWER_LEVELS[power_level_idx]);
	ImGui::Text("Cadence: %.0f rpm", my_bike->cadence * 60.f);
	ImGui::Text("Gear:    %d / %d   [%d-%d]",
		my_bike->gear.current_low_gear + 1,
		my_bike->gear.current_high_gear + 1,
		my_bike->gear.front_cogs[my_bike->gear.current_low_gear],
		my_bike->gear.back_cogs[my_bike->gear.current_high_gear]);
	const float eff_ws       = g_wind.wind_speed * (1.f + g_wind.wind_gust_factor * 1.5f);
	const glm::vec3 wdn      = glm::length(g_wind.wind_direction) > 0.001f ? glm::normalize(g_wind.wind_direction) : glm::vec3(0.f);
	const float wind_comp    = glm::dot(wdn, my_bike->bike_direction) * eff_ws;
	const float app_spd      = my_bike->speed - wind_comp;
	const float aero_drag_N  = 0.5f * 1.225f * 0.3f * app_spd * glm::abs(app_spd);
	ImGui::Text("Aero drag: %.1f N  (app wind %.1f m/s, %+.1f along)", aero_drag_N, eff_ws, wind_comp);
	ImGui::Text("Gradient:  %.1f%%  (%.1f deg)",
		glm::tan(my_bike->terrain_gradient) * 100.f,
		glm::degrees(my_bike->terrain_gradient));
	ImGui::Text("Steer committed: %.3f", my_bike->current_steer);
	ImGui::Text("[UP/DOWN] Power  [C/B] Coast  [SPACE] Brake  [V/X] Speed Hold");

	if (bo_for_debug) {
		const StaminaState& s = bo_for_debug->stamina;
		const RiderStats&   r = bo_for_debug->rider;
		ImGui::SeparatorText("Stamina");
		ImGui::Text("Glycogen:     %.1f%%   eff_FTP=%.0fW  (%s)",
			s.glycogen * 100.f, s.effective_ftp, s.legs_descriptor());
		ImGui::Text("W':           %.0f / %.0f J  (%d bars)  ceiling=%.0fW",
			s.w_prime, r.w_prime_max, s.w_prime_bars(r.w_prime_max), s.power_ceiling);
		ImGui::Text("HR:           %.0f bpm  (drift +%.1f  lactate +%.1f  heat +%.1f)  zone %d",
			s.hr_current, s.hr_drift, s.lactate * 0.002f, s.heat_stress * 20.f,
			s.hr_zone(r.hr_rest, r.hr_max));
		ImGui::Text("Heat stress:  %.1f%%   eff_FTP=%.0fW (heat factor %.2f)",
			s.heat_stress * 100.f, s.effective_ftp, 1.f - s.heat_stress * 0.12f);
	}

	ImGui::SeparatorText("Boids (player)");
	if (bp_for_debug && bo_for_debug) {
		const BikePlayer* bp = bp_for_debug;
		const BikeObject* bo = bo_for_debug;

		ImGui::Text("Steer final: %+.3f", bp->dbg_steer_final);
		ImGui::Text("Draft factor:        %.2f  (%.0f%% drag)", bo->draft_factor, bo->draft_factor * 100.f);
	}

	ImGui::SeparatorText("Steering");
	ImGui::DragFloat("steer_expo", &steer_expo, 0.05f, 1.f, 4.f);
	ImGui::Text("  half-stick → %.0f%% input  (1=linear, 2=quad, 3=cubic)", glm::pow(0.5f, steer_expo) * 100.f);

	ImGui::SeparatorText("Speed Hold");
	{
		ImGui::DragFloat("sh_power_up",   &sh_power_up,   0.001f, 0.001f, 1.f);
		ImGui::DragFloat("sh_power_down", &sh_power_down, 0.001f, 0.001f, 1.f);
		ImGui::DragFloat("sh_power_max",  &sh_power_max,  10.f,   0.f,   2000.f);
	}
	ImGui::SeparatorText("Sound");
	{
		ImGui::InputFloat("free_wheel_fade", &free_wheel_fade);
	}
}
ADD_TO_DEBUG_MENU(bike_status_debug);

// ============================================================
// Debug menu: Course / Race State
// ============================================================

static void bike_course_debug()
{
	if (!g_bike_app) return;
	const BikeCourse& c = g_bike_app->course;

	if (!c.is_built) {
		ImGui::TextColored({1,0.4f,0.4f,1}, "Course not built — no bike_waypoint spawners in level?");
		return;
	}

	ImGui::Text("Waypoints: %d   Length: %.0f m", (int)c.waypoints.size(), c.total_length_m);

	ImGui::SeparatorText("AI Count");
	ImGui::SliderInt("num_ai", &g_bike_app->num_ai, 0, 20);
	if (ImGui::Button("Respawn AI"))
		g_bike_app->respawn_ai();

	ImGui::SeparatorText("Wheel picker");
	{
		BikeAIParams& p = g_ai_params;
		ImGui::DragFloat("wheel_long_min",     &p.wheel_long_min,     0.05f, 0.f,   5.f, "%.2f");
		ImGui::DragFloat("wheel_long_max",     &p.wheel_long_max,     0.1f,  1.f,  20.f, "%.1f");
		ImGui::DragFloat("wheel_lat_max",      &p.wheel_lat_max,      0.05f, 0.5f,  6.f, "%.2f");
		ImGui::DragFloat("wheel_long_gap",     &p.wheel_long_gap,     0.05f, 0.5f, 10.f, "%.2f");
		ImGui::DragFloat("wheel_w_long",       &p.wheel_w_long,       0.05f, 0.f,   5.f, "%.2f");
		ImGui::DragFloat("wheel_w_lat",        &p.wheel_w_lat,        0.05f, 0.f,   5.f, "%.2f");
		ImGui::DragFloat("wheel_w_draft",      &p.wheel_w_draft,      0.05f, 0.f,   5.f, "%.2f");
		ImGui::DragFloat("wheel_stickiness",   &p.wheel_stickiness,   0.05f, 0.f,   3.f, "%.2f");
		ImGui::DragFloat("wheel_score_thresh", &p.wheel_score_thresh, 0.05f,-2.f,   2.f, "%.2f");
	}

	ImGui::SeparatorText("Clear-air resolver");
	{
		BikeAIParams& p = g_ai_params;
		ImGui::DragFloat("clear_air_lat_window",  &p.clear_air_lat_window,  0.05f, 0.1f, 3.f, "%.2f");
		ImGui::DragFloat("clear_air_long_window", &p.clear_air_long_window, 0.05f, 0.1f, 3.f, "%.2f");
		ImGui::DragFloat("clear_air_center_bias", &p.clear_air_center_bias, 0.01f, 0.f,  1.f, "%.2f");
		ImGui::DragFloat("clear_air_damp_tau",    &p.clear_air_damp_tau,    0.05f, 0.1f, 5.f, "%.2f");
		ImGui::DragFloat("clear_air_max_offset",  &p.clear_air_max_offset,  0.05f, 0.1f, 3.f, "%.2f");
		ImGui::DragFloat("clear_air_step",        &p.clear_air_step,        0.05f, 0.1f, 1.f, "%.2f");
		ImGui::DragFloat("corner_factor_r_full",  &p.corner_factor_r_full,  1.f,   5.f, 200.f, "%.0f");
		ImGui::DragFloat("corner_factor_min",     &p.corner_factor_min,     0.01f, 0.f,  1.f, "%.2f");
		ImGui::DragFloat("follower_lat_k",        &p.follower_lat_k,        0.05f, 0.f,  3.f, "%.2f");
		ImGui::SameLine(); ImGui::TextDisabled("near-field lat error → steer (followers)");
		ImGui::DragFloat("follower_lat_d_k",      &p.follower_lat_d_k,      0.02f, 0.f,  2.f, "%.2f");
		ImGui::SameLine(); ImGui::TextDisabled("damp lateral velocity");
	}

	ImGui::SeparatorText("Gap Regulation");
	{
		ImGui::DragFloat("GAP_POWER_K",         &GAP_POWER_K,         1.f,   0.f, 200.f, "%.0f");
		ImGui::SameLine(); ImGui::TextDisabled("W correction per metre of gap error");
		ImGui::DragFloat("GAP_POWER_MAX_DELTA", &GAP_POWER_MAX_DELTA, 5.f,  10.f, 600.f, "%.0f");
		ImGui::DragFloat("GAP_FREE_POWER_W",    &GAP_FREE_POWER_W,    5.f,  50.f, 800.f, "%.0f");
	}

	ImGui::SeparatorText("Paceline FSM");
	{
		BikeAIParams& p = g_ai_params;
		ImGui::DragFloat("pull_cooldown_s",    &p.pull_cooldown_s,    0.5f,  0.f, 60.f, "%.1f");
		ImGui::DragFloat("pull_duration_s",    &p.pull_duration_s,    1.f,   1.f,120.f, "%.0f");
		ImGui::DragFloat("peel_duration_s",    &p.peel_duration_s,    0.1f,  0.5f, 8.f, "%.1f");
		ImGui::DragFloat("drift_duration_s",   &p.drift_duration_s,   0.5f,  1.f, 30.f, "%.1f");
		ImGui::DragFloat("peel_offset_m",      &p.peel_offset_m,      0.05f, 0.1f, 3.f, "%.2f");
		ImGui::DragFloat("peel_power_delta_w", &p.peel_power_delta_w, 5.f, -200.f, 0.f, "%.0f");
		ImGui::DragFloat("drift_power_frac",   &p.drift_power_frac,   0.02f, 0.3f, 1.f, "%.2f");
		ImGui::DragFloat("pull_power_frac",    &p.pull_power_frac,    0.02f, 0.5f, 1.5f,"%.2f");
	}

	ImGui::SeparatorText("Riders");
	const auto& sorted = g_bike_app->riders_sorted;
	for (int i = 0; i < (int)sorted.size(); ++i) {
		const BikeObject* r = sorted[i];
		const BikeAI*     ai = dynamic_cast<const BikeAI*>(r->input.get());
		const char* mode = ai
		    ? (ai->wheel ? paceline_state_name(ai->paceline_state) : "LEAD")
		    : "player";
		ImGui::Text("P%d  dist=%.0f m  lat=%+.2f m  draft=%.2f  [%s]",
			r->race_position, r->course_dist_m, r->lateral_pos, r->draft_factor, mode);
	}

	ImGui::SeparatorText("Road Network");
	BikeCourse& course_rw = g_bike_app->course;
	ImGui::SliderFloat("Sample step (m)", &course_rw.sample_step_m, 0.2f, 5.f, "%.2f");

	ImGui::SeparatorText("Corner Fillets");
	ImGui::Checkbox("Fillet enabled", &course_rw.fillet_enabled);
	ImGui::DragFloat("Min angle (deg)", &course_rw.fillet_min_angle_deg, 0.5f, 0.f, 89.f, "%.1f");
	ImGui::Text("%d fillets active", (int)course_rw.debug_fillets.size());
	if (ImGui::Button("Rebuild Course"))
		g_bike_app->rebuild_course();

	ImGui::SeparatorText("Racing Line");
	bool rl_dirty = false;
	rl_dirty |= ImGui::DragFloat("RL k (stiffness)",   &course_rw.rl_k,            0.1f,  0.1f, 200.f,  "%.2f");
	rl_dirty |= ImGui::DragFloat("RL mass",            &course_rw.rl_mass,         1.f,   1.f,  500.f,  "%.1f");
	rl_dirty |= ImGui::DragInt  ("RL iterations",      &course_rw.rl_num_iters,    50,    100,  20000);
	rl_dirty |= ImGui::DragInt  ("RL smooth passes",   &course_rw.rl_smooth_passes, 1,     0,    100);
	ImGui::SameLine(); ImGui::TextDisabled("removes kinks from uneven waypoint spacing");
	rl_dirty |= ImGui::DragFloat("RL smooth weight",   &course_rw.rl_smooth_w,     0.01f, 0.f,  1.f,   "%.2f");
	if (rl_dirty)
		course_rw.rebuild_racing_line();
	if (ImGui::Button("Rebuild Racing Line"))
		course_rw.rebuild_racing_line();
	static bool draw_fillets = false;
	ImGui::Checkbox("Draw fillet geometry", &draw_fillets);
	if (draw_fillets)
		c.debug_draw_fillets();

	ImGui::SeparatorText("Visualisation");
	static bool draw_course         = true;
	static bool draw_projections    = true;   // sphere on spline at each rider's course_dist_m
	static bool draw_lookahead_all  = true;   // lookahead sphere for every rider
	ImGui::Checkbox("Draw course spline",          &draw_course);
	ImGui::Checkbox("Draw course projections",     &draw_projections);
	ImGui::Indent();
	ImGui::TextDisabled("Yellow = player  Orange = AI");
	ImGui::TextDisabled("Sphere on spline, line to actual position.");
	ImGui::TextDisabled("Gap shows projection error.");
	ImGui::Unindent();
	ImGui::Checkbox("Draw lookahead (all riders)", &draw_lookahead_all);
	ImGui::Indent();
	ImGui::TextDisabled("Cyan = player  Light cyan = AI");
	ImGui::Unindent();

	if (draw_course)
		c.debug_draw();

	// Lookahead parameters mirror BikeAI defaults so the player's dot is comparable.
	static constexpr float LOOK_BASE_M  = 10.f;
	static constexpr float LOOK_PER_MS  = 2.0f;
	static constexpr float CORNER_SCAN  = 50.f;
	static constexpr float CORNER_COEFF = 2.5f;

	for (auto* r : g_bike_app->all_riders) {
		const bool is_player = (dynamic_cast<BikePlayer*>(r->input.get()) != nullptr);

		if (draw_projections) {
			// Sphere ON the spline at course_dist_m. Distance from rider to this sphere
			// is the projection error — should be small except at wide racing-line offsets.
			const BikeWaypoint proj = c.sample(r->course_dist_m);
			const glm::vec3    proj_pos = proj.position + glm::vec3(0, 0.4f, 0);
			const Color32 col = is_player
			    ? Color32(0xff, 0xff, 0x00, 0xff)   // yellow  = player
			    : Color32(0xff, 0x88, 0x00, 0xff);  // orange  = AI
			Debug::add_sphere(proj_pos, 0.4f, col, -1.f);
			Debug::add_line(r->get_ws_position(), proj_pos,
			                Color32(col.r, col.g, col.b, 0x55), -1.f);
		}

		if (draw_lookahead_all) {
			glm::vec3 lookahead_pt;
			if (auto* ai = dynamic_cast<BikeAI*>(r->input.get())) {
				lookahead_pt = ai->dbg_lookahead_pt;  // already computed this frame
			} else {
				// Compute the same lookahead the AI would use, applied to the player.
				const float scan    = glm::max(CORNER_SCAN, r->speed * 0.8f);
				const float raw_r   = c.min_turn_radius_ahead(r->course_dist_m, scan);
				const float min_r   = glm::max(raw_r, 3.f);
				const float look_d  = glm::min(LOOK_BASE_M + r->speed * LOOK_PER_MS,
				                               CORNER_COEFF * min_r);
				lookahead_pt = c.racing_line_lookahead(r->course_dist_m, look_d);
			}
			const Color32 col = is_player
			    ? Color32(0x00, 0xff, 0xff, 0xff)   // cyan       = player
			    : Color32(0x88, 0xff, 0xff, 0xff);  // light cyan = AI
			Debug::add_sphere(lookahead_pt, 0.55f, col, -1.f);
			Debug::add_line(r->get_ws_position(), lookahead_pt,
			                Color32(col.r, col.g, col.b, 0x77), -1.f);
		}
	}
}
ADD_TO_DEBUG_MENU(bike_course_debug);

// ============================================================
// Debug camera follow state — file scope so update() can read it
// ============================================================
static int   g_follow_idx    = 0;
static float g_follow_dist   = 3.4f;
static float g_follow_height = 1.55f;
static float g_follow_pitch  = -20.f;

// Called by BikeGameApplication::update() after all rider updates
static void apply_debug_follow_camera()
{
	if (!g_follow_rider || !g_bike_app) return;
	const auto& all = g_bike_app->all_riders;
	if (all.empty()) return;
	const int idx = glm::clamp(g_follow_idx, 0, (int)all.size() - 1);
	BikeObject* bo = all[idx];

	CameraComponent* cc = CameraComponent::get_scene_camera();
	if (!cc) return;

	const glm::vec3 fwd      = glm::normalize(bo->bike_direction);
	const glm::vec3 world_up = glm::vec3(0, 1, 0);
	const glm::vec3 pivot    = bo->get_ws_position() + world_up * g_follow_height;
	const glm::vec3 right    = glm::normalize(glm::cross(fwd, world_up));

	const glm::quat pitch_rot = glm::angleAxis(glm::radians(g_follow_pitch), right);
	const glm::vec3 orbit_dir = glm::normalize(pitch_rot * (-fwd));
	const glm::vec3 cam_pos   = pivot + orbit_dir * g_follow_dist;

	const glm::vec3 look_at   = pivot + fwd * 3.f;
	const glm::vec3 cam_fwd   = glm::normalize(look_at - cam_pos);
	const glm::vec3 cam_right = glm::normalize(glm::cross(cam_fwd, world_up));
	const glm::vec3 cam_up    = glm::normalize(glm::cross(cam_right, cam_fwd));

	cc->get_owner()->set_ws_transform(glm::mat4(
		glm::vec4(cam_right, 0.f),
		glm::vec4(cam_up,    0.f),
		glm::vec4(-cam_fwd,  0.f),
		glm::vec4(cam_pos,   1.f)));

	BikePlayer* bp = dynamic_cast<BikePlayer*>(bo->input.get());
	BikeAI*     ai = dynamic_cast<BikeAI*>(bo->input.get());
	if (bp) {
		GameplayStatic::debug_text(string_format("[Player] steer_final:   %.2f", bp->dbg_steer_final));
	} else if (ai) {
		// --- Mode and path-following breakdown ---
		const char* ai_mode = ai->wheel ? paceline_state_name(ai->paceline_state) : "LEAD";
		GameplayStatic::debug_text(string_format("[AI:%s] spd=%.1fm/s  look=%.1fm  near_r=%.1fm  cf=%.2f",
			ai_mode, bo->speed, ai->dbg_lookahead_dist, ai->dbg_min_r, ai->dbg_corner_factor));
		GameplayStatic::debug_text(string_format("[AI] lat_off=%+.2f→%+.2f  bias=%+.2f  pace=%s t=%.1fs",
			ai->lat_offset, ai->dbg_lat_offset_target, ai->lat_offset_bias,
			paceline_state_name(ai->paceline_state), ai->paceline_timer_s));
		// Upcoming corner: distance, radius, safe speed, brake demand
		if (ai->dbg_brake_dist_m > 0.f)
			GameplayStatic::debug_text(string_format("[AI] corner in %.0fm  r=%.1fm  v_max=%.1fm/s  brake=%.2f",
				ai->dbg_brake_dist_m, ai->dbg_brake_corner_r, ai->dbg_v_max, ai->dbg_brake_amount));
		else
			GameplayStatic::debug_text(string_format("[AI] corner: clear for %.0fm",
				(float)(ai->BRAKE_SCAN_STEPS * ai->BRAKE_SCAN_STEP_M)));
		GameplayStatic::debug_text(string_format("[AI] steer: near=%+.2f  far=%+.2f  edge=%+.2f  avoid=%+.2f  final=%+.2f",
			ai->dbg_steer_near, ai->dbg_steer_far, ai->dbg_edge_steer, ai->dbg_avoid_steer, ai->dbg_steer_final));
		if (ai->dbg_avoid_brake > 0.f || ai->hard_steer_min > -1.f || ai->hard_steer_max < 1.f)
			GameplayStatic::debug_text(string_format("[AI] YIELD: brake=%.2f  steer_min=%+.1f  steer_max=%+.1f",
				ai->dbg_avoid_brake, ai->hard_steer_min, ai->hard_steer_max));
		GameplayStatic::debug_text(string_format("[AI] power base:%.0fW  sep:%+.0fW  cmd=%.0fW  actual=%.0fW",
			ai->dbg_power_base, bo->boid_long_sep_power, ai->dbg_power_final, bo->stamina.actual_power));

		// --- Visual overlays for selected AI ---
		// Lookahead point + line
		Debug::add_sphere(ai->dbg_lookahead_pt, 0.35f, Color32(0x00, 0xff, 0xff, 0xff), -1.f);
		Debug::add_line(bo->get_ws_position(), ai->dbg_lookahead_pt, Color32(0x00, 0xff, 0xff, 0xaa), -1.f);

		// Racing line reference point at the bike's current course position (lateral error line)
		if (g_bike_app && g_bike_app->course.is_built) {
			const BikeWaypoint cur_wp = g_bike_app->course.sample(bo->course_dist_m);
			const glm::vec3 rl_ref = cur_wp.racing_line_pos;
			// White dot on the racing line closest to the bike, red line showing lateral error
			Debug::add_sphere(rl_ref, 0.25f, Color32(0xff, 0xff, 0xff, 0xff), -1.f);
			const glm::vec3 bike_pos = bo->get_ws_position();
			const glm::vec3 bike_on_road = cur_wp.position + cur_wp.right * bo->lateral_pos;
			Debug::add_line(bike_on_road, rl_ref, Color32(0xff, 0x33, 0x33, 0xff), -1.f);
		}
	}
}

// ============================================================
// Rider snapshot — record all riders and teleport back
// ============================================================

struct BikeRiderSnapshot {
	glm::vec3 position;
	glm::vec3 bike_direction;
	float speed;
	float course_dist_m;
	float lateral_pos;
	int   course_segment;
	float actual_power_command;  // AI only; ignored for player
};

static std::vector<BikeRiderSnapshot> s_rider_snapshots;

static void snapshot_record()
{
	if (!g_bike_app) return;
	s_rider_snapshots.clear();
	for (BikeObject* bo : g_bike_app->all_riders) {
		BikeRiderSnapshot snap;
		snap.position             = bo->get_ws_position();
		snap.bike_direction       = bo->bike_direction;
		snap.speed                = bo->speed;
		snap.course_dist_m        = bo->course_dist_m;
		snap.lateral_pos          = bo->lateral_pos;
		snap.course_segment       = bo->course_segment;
		snap.actual_power_command = 0.f;
		if (auto* ai = dynamic_cast<BikeAI*>(bo->input.get()))
			snap.actual_power_command = ai->actual_power_command;
		s_rider_snapshots.push_back(snap);
	}
}

static void snapshot_restore()
{
	if (!g_bike_app || s_rider_snapshots.empty()) return;
	const int n = (int)glm::min(g_bike_app->all_riders.size(), s_rider_snapshots.size());
	for (int i = 0; i < n; ++i) {
		BikeObject* bo           = g_bike_app->all_riders[i];
		const BikeRiderSnapshot& snap = s_rider_snapshots[i];
		bo->get_owner()->set_ws_position(snap.position);
		bo->bike_direction  = snap.bike_direction;
		bo->speed           = snap.speed;
		bo->course_dist_m   = snap.course_dist_m;
		bo->lateral_pos     = snap.lateral_pos;
		bo->course_segment  = snap.course_segment;
		// Reset transient physics state so the bike doesn't carry over a crash or spin
		bo->current_steer   = 0.f;
		bo->steer_committed = 0.f;
		bo->is_crashed      = false;
		bo->crash_timer     = 0.f;
		if (auto* ai = dynamic_cast<BikeAI*>(bo->input.get()))
			ai->actual_power_command = snap.actual_power_command;
	}
}

// ============================================================
// Debug menu: Boid visualisation
// ============================================================

static void bike_boid_debug()
{
	if (!g_bike_app) return;
	const auto& all    = g_bike_app->all_riders;
	const auto& sorted = g_bike_app->riders_sorted;
	if (all.empty()) return;

	// ---- Snapshot ----
	ImGui::SeparatorText("Segment Replay");
	if (ImGui::Button("Record positions"))
		snapshot_record();
	ImGui::SameLine();
	const bool has_snap = !s_rider_snapshots.empty();
	if (!has_snap) ImGui::BeginDisabled();
	if (ImGui::Button("Teleport to recorded"))
		snapshot_restore();
	if (!has_snap) ImGui::EndDisabled();
	if (has_snap)
		ImGui::SameLine(), ImGui::TextDisabled("(%d riders saved)", (int)s_rider_snapshots.size());

	// ---- Rider picker ----
	static int  selected_idx  = 0;          // index into all_riders
	static bool draw_boids    = true;

	ImGui::Checkbox("Draw boid debug", &draw_boids);

	// Build label list: "Player" for BikePlayer, "AI #N" for BikeAI
	ImGui::Text("Select rider:");
	ImGui::SameLine();
	ImGui::InputInt("##label", &selected_idx,1);	
	selected_idx = glm::clamp(selected_idx, 0, (int)all.size() - 1);
	BikeObject* bo = all[selected_idx];

	// ---- Camera follow ----
	g_follow_idx = selected_idx;
	ImGui::Checkbox("Follow selected rider (camera)", &g_follow_rider);
	if (g_follow_rider) {
		ImGui::SetNextItemWidth(70.f); ImGui::DragFloat("dist",   &g_follow_dist,   0.05f, 0.5f, 10.f);
		ImGui::SetNextItemWidth(70.f); ImGui::DragFloat("height", &g_follow_height, 0.05f, 0.5f,  5.f);
		ImGui::SetNextItemWidth(70.f); ImGui::DragFloat("pitch°", &g_follow_pitch,  0.5f,  0.f, 80.f);
	}

	// ---- Text readout ----
	ImGui::Separator();
	ImGui::Text("long_sep_power:    %+.1f W", bo->boid_long_sep_power);

	// ---- Tuning sliders ----
	ImGui::SeparatorText("Side-by-side power yield");
	ImGui::DragFloat("long m",     &SIDE_BY_SIDE_LONG_M,  0.05f, 0.5f, 10.f);
	ImGui::DragFloat("lat m",      &SIDE_BY_SIDE_LAT_M,   0.05f, 0.1f,  3.f);
	ImGui::DragFloat("power max W",&SIDE_BY_SIDE_POWER_W, 5.f,   0.f, 200.f);

	ImGui::SeparatorText("AI Cornering");
	ImGui::DragFloat("lookahead_dist_base",   &g_ai_params.lookahead_dist_base,   0.1f,  0.1f, 20.f);
	ImGui::SameLine(); ImGui::TextDisabled("base lookahead m (+ speed*per_ms)");
	ImGui::DragFloat("lookahead_dist_per_ms", &g_ai_params.lookahead_dist_per_ms, 0.05f, 0.f,  3.f);
	ImGui::DragFloat("steer_k",               &g_ai_params.steer_k,               0.1f,  0.1f, 10.f);
	ImGui::SameLine(); ImGui::TextDisabled("lateral error gain");
	ImGui::DragFloat("corner_look_m",         &g_ai_params.corner_look_m,         1.f,   5.f, 120.f);
	ImGui::DragFloat("corner_speed_k",        &g_ai_params.corner_speed_k,        0.05f, 0.1f,  5.f);
	ImGui::SameLine(); ImGui::TextDisabled("v_max=sqrt(k*g*R)");
	ImGui::DragFloat("anticipation dist scale", &g_ai_params.anticipation_dist_scale, 0.1f, 1.f, 5.f);
	ImGui::SameLine(); ImGui::TextDisabled("far lookahead = near * this");
	ImGui::DragFloat("anticipation k",          &g_ai_params.anticipation_k,          0.05f, 0.f, 1.f);
	ImGui::SameLine(); ImGui::TextDisabled("0=disabled 1=all-far");

	ImGui::SeparatorText("AI Boundary Avoidance");
	ImGui::DragFloat("edge predict t", &g_ai_params.edge_predict_t, 0.05f, 0.1f, 3.f);
	ImGui::SameLine(); ImGui::TextDisabled("seconds ahead to project arc");
	ImGui::DragFloat("edge safety m",  &g_ai_params.edge_safety_m,  0.05f, 0.f,  2.f);
	ImGui::SameLine(); ImGui::TextDisabled("margin inside road edge");
	ImGui::DragFloat("edge steer k",   &g_ai_params.edge_steer_k,   0.05f, 0.f,  5.f);
	ImGui::SameLine(); ImGui::TextDisabled("P gain: steer per metre beyond safe zone");
	ImGui::DragFloat("edge vel damp",  &g_ai_params.edge_vel_damp,  0.05f, 0.f,  3.f);
	ImGui::SameLine(); ImGui::TextDisabled("D gain: damp correction when already returning");
	ImGui::DragFloat("edge off brake k",   &g_ai_params.edge_off_brake_k,   0.05f, 0.f, 3.f);
	ImGui::SameLine(); ImGui::TextDisabled("brake fraction per metre past road edge");
	ImGui::DragFloat("edge off brake max", &g_ai_params.edge_off_brake_max, 0.05f, 0.f, 1.f);
	ImGui::SameLine(); ImGui::TextDisabled("max brake fraction during off-track recovery");

	ImGui::SeparatorText("Soft Lateral Separation");
	ImGui::DragFloat("avoid long max m",  &AVOID_LONG_MAX,    0.1f,  0.5f, 10.f);
	ImGui::DragFloat("avoid half-w m",    &AVOID_BIKE_HALF_W, 0.01f, 0.1f,  1.f);
	ImGui::DragFloat("avoid clearance m", &AVOID_CLEARANCE,   0.01f, 0.f,   1.f);
	ImGui::DragFloat("avoid predict t1",  &AVOID_PREDICT_T1,  0.05f, 0.f,   3.f);
	ImGui::DragFloat("avoid predict t2",  &AVOID_PREDICT_T2,  0.05f, 0.f,   3.f);
	ImGui::DragFloat("avoid steer kp",    &AVOID_STEER_KP,    0.05f, 0.f,   5.f);

	ImGui::SeparatorText("Priority Yield (Hard Clamp)");
	ImGui::DragFloat("yield long radius m", &YIELD_LONG_RADIUS, 0.1f, 0.5f, 10.f);
	ImGui::DragFloat("yield outer lat m",   &YIELD_OUTER_LAT,   0.05f, 0.1f,  3.f);
	ImGui::DragFloat("yield inner lat m",   &YIELD_INNER_LAT,   0.01f, 0.f,   0.5f);
	ImGui::DragFloat("yield squeeze m",     &YIELD_SQUEEZE_M,   0.05f, 0.f,   2.f);
	ImGui::DragFloat("yield brake k",       &YIELD_BRAKE_K,     0.05f, 0.f,   1.f);

	if (!draw_boids) return;

	// ============================================================
	// World-space drawing
	// ============================================================
	const glm::vec3 my_pos   = bo->get_ws_position();
	const glm::vec3 up        = glm::vec3(0, 0.05f, 0);  // slight y offset so lines clear the ground
	const glm::vec3 right_ws  = glm::normalize(glm::cross(bo->bike_direction, glm::vec3(0, 1, 0)));

	// Highlight selected rider with a white sphere
	Debug::add_sphere(my_pos + glm::vec3(0, 1.2f, 0), 0.25f, COLOR_WHITE, -1.f);

	// ---- Draft factor bar — horizontal line above rider, length = (1-draft_factor)*4 ----
	{
		const float benefit_len = (1.f - bo->draft_factor) * 4.f;  // 0 = no draft, up to ~1.4m at full draft
		if (benefit_len > 0.01f) {
			const glm::vec3 bar_start = my_pos + glm::vec3(0, 2.f, 0) - right_ws * benefit_len * 0.5f;
			const glm::vec3 bar_end   = bar_start + right_ws * benefit_len;
			Debug::add_line(bar_start, bar_end, Color32(0xff, 0xff, 0x00, 0xff), -1.f);  // yellow = draft
		}
	}
}
ADD_TO_DEBUG_MENU(bike_boid_debug);

// ============================================================
// Crack trigger: fire bump FX when a rider rolls over a crack decal
// ============================================================

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

// ============================================================
// Debug menu: AI Debug Recorder
// ============================================================

static void ai_recorder_debug_menu()
{
	if (!g_bike_app) return;

	ImGui::SeparatorText("AI Debug Recorder");

	const int   frame_count  = (int)s_ai_dbg_frames.size();
	const int   rider_count  = (int)g_bike_app->all_riders.size();
	const float duration     = (frame_count > 0)
		? s_ai_dbg_frames.back().time_s - s_ai_dbg_frames.front().time_s : 0.f;

	if (s_ai_dbg_full)
		ImGui::TextColored({1.f, 0.4f, 0.f, 1.f},
			"Buffer full (%d frames). Dump or clear.", AI_DBG_MAX_FRAMES);
	else
		ImGui::Text("Frames: %d / %d   Duration: %.1f s   Interval: %.0f ms",
			frame_count, AI_DBG_MAX_FRAMES, duration, AI_DBG_INTERVAL * 1000.f);

	if (!s_ai_dbg_recording) {
		if (ImGui::Button("Start Recording")) {
			s_ai_dbg_recording = true;
			s_ai_dbg_full      = false;
			s_ai_dbg_accum     = 0.f;
		}
	} else {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.f));
		if (ImGui::Button("Stop Recording"))
			s_ai_dbg_recording = false;
		ImGui::PopStyleColor();
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear")) {
		s_ai_dbg_frames.clear();
		s_ai_dbg_frames.shrink_to_fit();
		s_ai_dbg_time      = 0.f;
		s_ai_dbg_accum     = 0.f;
		s_ai_dbg_full      = false;
		s_ai_dbg_recording = false;
	}
	ImGui::SameLine();
	if (frame_count > 0) {
		if (ImGui::Button("Dump to CSV"))
			ai_debug_dump(g_bike_app);
	} else {
		ImGui::BeginDisabled();
		ImGui::Button("Dump to CSV");
		ImGui::EndDisabled();
	}
	ImGui::TextDisabled("Outputs: D:/Data/ai_debug_dump.csv  +  ai_debug_course.csv");

	// Course / racing-line audit — independent of AI recording.
	if (ImGui::Button("Dump course audit CSV")) {
		if (g_bike_app && g_bike_app->course.is_built)
			g_bike_app->course.dump_audit_csv("D:/Data/bike_course_audit.csv");
	}
	ImGui::SameLine();
	ImGui::TextDisabled("D:/Data/bike_course_audit.csv");

	if (frame_count == 0) return;

	// Per-rider stats
	ImGui::SeparatorText("Per-Rider Stats");
	static int s_stats_rider = 0;
	ImGui::SetNextItemWidth(80.f);
	ImGui::InputInt("Rider idx", &s_stats_rider, 1);
	s_stats_rider = glm::clamp(s_stats_rider, 0, rider_count - 1);

	float lat_min   = FLT_MAX, lat_max   = -FLT_MAX, lat_sum   = 0.f;
	float err_min   = FLT_MAX, err_max   = -FLT_MAX, err_sum   = 0.f;
	float spd_min   = FLT_MAX, spd_max   = -FLT_MAX;
	float str_min   = FLT_MAX, str_max   = -FLT_MAX;
	float sep_min   = FLT_MAX;
	int   off_count = 0, col_count = 0, crash_count = 0, rider_frames = 0;

	for (const auto& fr : s_ai_dbg_frames) {
		if (fr.rider_idx != s_stats_rider) continue;
		++rider_frames;
		lat_min    = glm::min(lat_min, fr.lateral_pos);
		lat_max    = glm::max(lat_max, fr.lateral_pos);
		lat_sum   += fr.lateral_pos;
		err_min    = glm::min(err_min, fr.lat_err);
		err_max    = glm::max(err_max, fr.lat_err);
		err_sum   += fr.lat_err;
		spd_min    = glm::min(spd_min, fr.speed_ms);
		spd_max    = glm::max(spd_max, fr.speed_ms);
		str_min    = glm::min(str_min, fr.steer_final);
		str_max    = glm::max(str_max, fr.steer_final);
		sep_min    = glm::min(sep_min, fr.nearest_lat_sep_m);
		if (fr.is_off_track) ++off_count;
		if (fr.is_colliding) ++col_count;
		if (fr.is_crashed)   ++crash_count;
	}

	if (rider_frames == 0) {
		ImGui::TextDisabled("No frames recorded for rider %d", s_stats_rider);
		return;
	}

	const float lat_mean = lat_sum / (float)rider_frames;
	const float err_mean = err_sum / (float)rider_frames;
	const bool  is_ai    = [&]() {
		for (const auto& fr : s_ai_dbg_frames)
			if (fr.rider_idx == s_stats_rider) return fr.is_ai;
		return false;
	}();

	ImGui::Text("Type: %s   Frames: %d", is_ai ? "AI" : "Player", rider_frames);
	ImGui::Text("Speed (m/s):     min=%.2f  max=%.2f", spd_min, spd_max);
	ImGui::Text("Lateral pos (m): min=%+.2f  max=%+.2f  mean=%+.2f", lat_min, lat_max, lat_mean);
	ImGui::Text("Lat err vs RL:   min=%+.2f  max=%+.2f  mean=%+.2f", err_min, err_max, err_mean);
	if (is_ai)
		ImGui::Text("Steer final:     min=%+.2f  max=%+.2f", str_min, str_max);
	ImGui::Text("Min lat sep (m): %.3f", sep_min < 999.f ? sep_min : -1.f);

	if (off_count > 0)
		ImGui::TextColored({1.f, 0.4f, 0.4f, 1.f},
			"Off-track: %d / %d (%.1f%%)", off_count, rider_frames, off_count * 100.f / rider_frames);
	else
		ImGui::TextDisabled("Off-track: 0 frames");

	if (col_count > 0)
		ImGui::TextColored({1.f, 0.7f, 0.f, 1.f},
			"Colliding: %d / %d (%.1f%%)", col_count, rider_frames, col_count * 100.f / rider_frames);
	else
		ImGui::TextDisabled("Colliding: 0 frames");

	if (crash_count > 0)
		ImGui::TextColored({1.f, 0.2f, 0.2f, 1.f},
			"Crashed:   %d / %d (%.1f%%)", crash_count, rider_frames, crash_count * 100.f / rider_frames);
	else
		ImGui::TextDisabled("Crashed:   0 frames");
}
ADD_TO_DEBUG_MENU(ai_recorder_debug_menu);
