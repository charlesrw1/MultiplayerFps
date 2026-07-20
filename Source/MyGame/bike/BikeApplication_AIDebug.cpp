// BikeApplication_AIDebug.cpp
// AI debug recorder: frame capture struct, recording state, record/dump helpers.
// Called from BikeGameApplication::update() and the AI recorder debug menu.

#include "BikeHeaders.h"

#include "GameEnginePublic.h"
#include "Framework/MathLib.h"
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>

// Forward declarations
extern BikeGameApplication* g_bike_app;

// ============================================================
// AI Debug Recorder
// ============================================================

struct AIDebugFrame {
    float time_s;
    int   rider_idx;
    bool  is_ai;
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

    // Steering / magnetism (AI only; 0 for player)
    float steer_final;
    int   num_neighbors;
    float cohesion_offset;
    float separation_offset;
    float draft_offset;
    float lineform_offset;
    float target_lat_offset;
    int   clamped;
    float lateral_shift;

    // Braking
    float brake_amount;
    float brake_dist_m;   // distance to braking corner
    float v_max_corner;   // safe speed for that corner
    float brake_corner_r;
    float min_r;          // tightest radius in scan window

    // Power
    float power_final;
    float target_speed;

    // Nearest rider (longitudinally closest within 10 m)
    float nearest_lat_sep_m;
    float nearest_long_gap_m;

    // Lookahead point fed to the steer PID (BikeAI::dbg_lookahead_pt)
    float lookahead_pt_x, lookahead_pt_y, lookahead_pt_z;
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

        if (ai) {
            fr.steer_final       = ai->dbg_steer_final;
            fr.num_neighbors     = ai->dbg_num_neighbors;
            fr.cohesion_offset   = ai->dbg_cohesion_offset;
            fr.separation_offset = ai->dbg_separation_offset;
            fr.draft_offset      = ai->dbg_draft_offset;
            fr.lineform_offset   = ai->dbg_lineform_offset;
            fr.target_lat_offset = ai->dbg_target_lat_offset;
            fr.clamped           = ai->dbg_clamped ? 1 : 0;
            fr.lateral_shift     = ai->dbg_lateral_shift;
            fr.brake_amount      = ai->dbg_brake_amount;
            fr.brake_dist_m      = ai->dbg_brake_dist_m;
            fr.v_max_corner      = ai->dbg_v_max;
            fr.brake_corner_r    = ai->dbg_brake_corner_r;
            fr.min_r             = ai->dbg_min_r;
            fr.power_final       = ai->dbg_power_final;
            fr.target_speed      = ai->dbg_target_speed;
            fr.lookahead_pt_x    = ai->dbg_lookahead_pt.x;
            fr.lookahead_pt_y    = ai->dbg_lookahead_pt.y;
            fr.lookahead_pt_z    = ai->dbg_lookahead_pt.z;
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
        f << "time_s,rider_idx,is_ai,is_off_track,is_colliding,"
             "course_dist_m,lateral_pos,lateral_vel,speed_ms,heading_deg,"
             "road_half_width,rl_lateral,lat_err,"
             "steer_final,num_neighbors,cohesion_offset,separation_offset,draft_offset,lineform_offset,"
             "target_lat_offset,clamped,lateral_shift,"
             "brake_amount,brake_dist_m,v_max_corner,brake_corner_r,min_r,"
             "power_final,target_speed,"
             "nearest_lat_sep_m,nearest_long_gap_m,"
             "lookahead_pt_x,lookahead_pt_y,lookahead_pt_z\n";
        for (const auto& fr : s_ai_dbg_frames) {
            f << fr.time_s              << ','
              << fr.rider_idx           << ',' << (int)fr.is_ai         << ','
              << (int)fr.is_off_track   << ',' << (int)fr.is_colliding  << ','
              << fr.course_dist_m       << ',' << fr.lateral_pos        << ',' << fr.lateral_vel      << ','
              << fr.speed_ms            << ',' << fr.heading_deg        << ','
              << fr.road_half_width     << ',' << fr.rl_lateral         << ',' << fr.lat_err           << ','
              << fr.steer_final         << ',' << fr.num_neighbors      << ','
              << fr.cohesion_offset     << ',' << fr.separation_offset  << ',' << fr.draft_offset      << ','
              << fr.lineform_offset     << ',' << fr.target_lat_offset  << ',' << fr.clamped           << ','
              << fr.lateral_shift       << ','
              << fr.brake_amount        << ',' << fr.brake_dist_m       << ','
              << fr.v_max_corner        << ',' << fr.brake_corner_r     << ',' << fr.min_r             << ','
              << fr.power_final         << ',' << fr.target_speed       << ','
              << fr.nearest_lat_sep_m   << ',' << fr.nearest_long_gap_m << ','
              << fr.lookahead_pt_x      << ',' << fr.lookahead_pt_y     << ',' << fr.lookahead_pt_z << '\n';
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

// ============================================================
// Public entry points called from BikeApplication.cpp
// ============================================================

void ai_debug_update(BikeGameApplication* app, float dt)
{
    if (!s_ai_dbg_recording || s_ai_dbg_full) return;
    s_ai_dbg_accum += dt;
    s_ai_dbg_time  += dt;
    if (s_ai_dbg_accum >= AI_DBG_INTERVAL) {
        ai_debug_record(app);
        s_ai_dbg_accum -= AI_DBG_INTERVAL;
    }
}

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
	int   off_count = 0, col_count = 0, clamp_count = 0, rider_frames = 0;

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
		if (fr.clamped)      ++clamp_count;
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

	if (clamp_count > 0)
		ImGui::TextColored({1.f, 0.6f, 0.f, 1.f},
			"Edge-clamped: %d / %d (%.1f%%)", clamp_count, rider_frames, clamp_count * 100.f / rider_frames);
	else
		ImGui::TextDisabled("Edge-clamped: 0 frames");
}
ADD_TO_DEBUG_MENU(ai_recorder_debug_menu);
