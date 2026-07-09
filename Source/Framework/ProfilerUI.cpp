// Flax-Engine-style profiler window: left sidebar (Overall/CPU/GPU), a
// scrubbable sparkline strip, a flame graph, and a collapsible per-frame
// timing table. See Source/Framework/Profiler.h for the underlying capture.
#include "Framework/ProfilerUI.h"
#include "Framework/Profiler.h"
#include "Framework/Config.h"
#include "imgui.h"
#include <cstdint>
#include <cstdio>
#include <cfloat>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <functional>

using namespace prof;

ConfigVar stat_profiler("stat.profiler", "0", CVAR_BOOL, "show the profiler window (Overall/CPU/GPU tabs)");

namespace {

enum class ProfTab { Overall, Cpu, Gpu };
ProfTab g_tab           = ProfTab::Overall;
uint64_t g_scrub_frame  = 0;
bool g_follow_live      = true;

double ns_to_ms(uint64_t ns) { return ns / 1'000'000.0; }
bool is_resolved(const ZoneEvent& e) { return e.start_ns != UINT64_MAX && e.end_ns != UINT64_MAX; }

const FrameEvents* find_frame(const std::vector<FrameEvents>& ring, uint64_t frame_index) {
	if (ring.empty())
		return nullptr;
	const size_t idx = frame_index % ring.size();
	if (ring[idx].frame_index != frame_index)
		return nullptr;
	return &ring[idx];
}

// First matching top-level-or-nested zone by name within one frame; 0 if absent/unresolved.
double find_zone_ms(const FrameEvents* fe, const char* name) {
	if (!fe)
		return 0.0;
	for (auto& e : fe->events) {
		if (!is_resolved(e))
			continue;
		if (ProfilerRegistry::location(e.slot).name == name)
			return ns_to_ms(e.end_ns - e.start_ns);
	}
	return 0.0;
}

// Whole-frame wall span: min start .. max end across every resolved event.
double frame_span_ms(const FrameEvents* fe) {
	if (!fe || fe->events.empty())
		return 0.0;
	uint64_t lo = UINT64_MAX, hi = 0;
	for (auto& e : fe->events) {
		if (!is_resolved(e))
			continue;
		lo = std::min(lo, e.start_ns);
		hi = std::max(hi, e.end_ns);
	}
	return lo <= hi ? ns_to_ms(hi - lo) : 0.0;
}

ImU32 zone_color(uint32_t slot) {
	// Golden-ratio hash -> stable, well-spread hue per call site.
	const float hue = fmodf(slot * 0.6180339887f, 1.0f);
	ImVec4 c        = ImColor::HSV(hue, 0.55f, 0.85f);
	return ImGui::ColorConvertFloat4ToU32(c);
}

// ---- Toolbar ------------------------------------------------------------

void draw_toolbar() {
	const RecordingState state = Profiler::recording_state();

	const char* record_label = state == RecordingState::Recording ? "Pause"
									 : state == RecordingState::Paused ? "Resume"
																	   : "Record";
	if (ImGui::Button(record_label)) {
		const RecordingState next =
			state == RecordingState::Recording ? RecordingState::Paused : RecordingState::Recording;
		Profiler::set_recording_state(next);
		if (next == RecordingState::Recording)
			g_follow_live = true; // resuming/starting playback should track the live frame again
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear")) {
		Profiler::clear_history();
		g_follow_live = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("<")) {
		g_follow_live = false;
		if (g_scrub_frame > 0)
			g_scrub_frame--;
	}
	ImGui::SameLine();
	if (ImGui::Button(">")) {
		g_follow_live = false;
		g_scrub_frame++;
	}
	ImGui::SameLine();
	if (ImGui::Button("Live"))
		g_follow_live = true;

	ImGui::SameLine();
	ImGui::TextDisabled(state == RecordingState::Recording	 ? "(recording)"
						 : state == RecordingState::Paused	 ? "(paused)"
																 : "(live)");
}

// ---- Scrubbable sparkline strip -----------------------------------------

constexpr uint64_t kNoFrame = UINT64_MAX;

// Builds a fixed-width, right-aligned window of frame indices: the newest
// committed frame always lands in the last slot, and slots before frame 0
// (or before recording started) are kNoFrame. Fixed width means a
// half-full buffer draws as data scrolling in from the right rather than
// stretching a handful of samples across the whole plot.
std::vector<uint64_t> display_window(size_t capacity, uint64_t upto_exclusive) {
	std::vector<uint64_t> out(capacity, kNoFrame);
	for (size_t slot = 0; slot < capacity; slot++) {
		const uint64_t offset_from_newest = capacity - 1 - slot;
		if (upto_exclusive == 0 || offset_from_newest >= upto_exclusive)
			continue;
		out[slot] = upto_exclusive - 1 - offset_from_newest;
	}
	return out;
}

// Draws one labeled sparkline over a fixed-width window of frame ids
// (kNoFrame slots plot as 0). Click/drag anywhere on it moves the shared
// scrub cursor.
void draw_sparkline(const char* label, const std::vector<uint64_t>& frame_ids,
					 const std::function<double(uint64_t)>& metric) {
	std::vector<float> fvals(frame_ids.size(), 0.0f);
	double overlay_current = 0.0;
	for (size_t i = 0; i < frame_ids.size(); i++) {
		if (frame_ids[i] == kNoFrame)
			continue;
		fvals[i] = (float)metric(frame_ids[i]);
		overlay_current = fvals[i];
	}

	char overlay[64];
	snprintf(overlay, sizeof(overlay), "%s: %.2f", label, overlay_current);

	ImGui::PlotLines(("##" + std::string(label)).c_str(), fvals.empty() ? nullptr : fvals.data(), (int)fvals.size(),
					  0, overlay, 0.0f, FLT_MAX, ImVec2(-1, 40));

	const ImVec2 rmin = ImGui::GetItemRectMin();
	const ImVec2 rmax = ImGui::GetItemRectMax();

	if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !frame_ids.empty()) {
		const float t  = std::clamp((ImGui::GetMousePos().x - rmin.x) / std::max(1.0f, rmax.x - rmin.x), 0.0f, 1.0f);
		const size_t i = std::min(frame_ids.size() - 1, (size_t)(t * frame_ids.size()));
		if (frame_ids[i] != kNoFrame) {
			g_scrub_frame = frame_ids[i];
			g_follow_live = false;
		}
	}

	if (!frame_ids.empty()) {
		const uint64_t shown = g_follow_live ? frame_ids.back() : g_scrub_frame;
		auto it				  = std::find(frame_ids.begin(), frame_ids.end(), shown);
		if (it != frame_ids.end() && shown != kNoFrame) {
			const float t = (frame_ids.size() > 1) ? (float)(it - frame_ids.begin()) / (float)(frame_ids.size() - 1)
													: 0.0f;
			const float x = rmin.x + t * (rmax.x - rmin.x);
			ImGui::GetWindowDrawList()->AddLine(ImVec2(x, rmin.y), ImVec2(x, rmax.y), IM_COL32(255, 255, 255, 200));
		}
	}
}

// ---- Flame graph ---------------------------------------------------------

void draw_flame_lane(const char* lane_name, const FrameEvents* fe) {
	if (!fe || fe->events.empty()) {
		ImGui::TextDisabled("%s: (no data)", lane_name);
		return;
	}

	uint64_t tmin = UINT64_MAX, tmax = 0;
	uint16_t max_depth = 0;
	for (auto& e : fe->events) {
		if (!is_resolved(e))
			continue;
		tmin	  = std::min(tmin, e.start_ns);
		tmax	  = std::max(tmax, e.end_ns);
		max_depth = std::max(max_depth, e.depth);
	}
	if (tmin > tmax) {
		ImGui::TextDisabled("%s: (unresolved)", lane_name);
		return;
	}

	ImGui::TextUnformatted(lane_name);
	const float row_h		 = 20.0f;
	const float lane_h		 = (max_depth + 1) * row_h + 4;
	const ImVec2 origin		 = ImGui::GetCursorScreenPos();
	const float width		 = std::max(50.0f, ImGui::GetContentRegionAvail().x);
	const double span_ns	 = double(tmax - tmin);
	ImDrawList* dl			 = ImGui::GetWindowDrawList();

	dl->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + lane_h), IM_COL32(30, 30, 30, 255));

	for (auto& e : fe->events) {
		if (!is_resolved(e) || span_ns <= 0.0)
			continue;
		const float x0 = origin.x + (float)((e.start_ns - tmin) / span_ns) * width;
		const float x1 = std::max(x0 + 1.0f, origin.x + (float)((e.end_ns - tmin) / span_ns) * width);
		const float y0 = origin.y + e.depth * row_h;
		const float y1 = y0 + row_h - 1;

		const ImU32 col = zone_color(e.slot);
		dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), col);
		dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(0, 0, 0, 90));

		const auto& loc = ProfilerRegistry::location(e.slot);
		if (x1 - x0 > 24.0f) {
			ImGui::PushClipRect(ImVec2(x0, y0), ImVec2(x1, y1), true);
			dl->AddText(ImVec2(x0 + 2, y0 + 2), IM_COL32(0, 0, 0, 255), loc.name.c_str());
			ImGui::PopClipRect();
		}

		if (ImGui::IsMouseHoveringRect(ImVec2(x0, y0), ImVec2(x1, y1))) {
			ImGui::BeginTooltip();
			ImGui::Text("%s: %.3f ms", loc.name.c_str(), ns_to_ms(e.end_ns - e.start_ns));
			ImGui::EndTooltip();
		}
	}

	ImGui::Dummy(ImVec2(width, lane_h));
}

// ---- Per-frame collapsible timing table ----------------------------------

// Draws the subtree of `fe.events[start..end_bound)` at exactly `depth`,
// recursing into children. Returns the index just past this subtree.
uint32_t draw_zone_rows(const FrameEvents& fe, uint32_t start, uint32_t end_bound, uint16_t depth,
						 double frame_total_ns) {
	uint32_t i = start;
	while (i < end_bound && fe.events[i].depth == depth) {
		const uint32_t node_index = i;
		const uint32_t child_start = i + 1;
		uint32_t subtree_end	   = child_start;
		while (subtree_end < end_bound && fe.events[subtree_end].depth > depth)
			subtree_end++;

		const ZoneEvent& e = fe.events[node_index];
		const bool resolved = is_resolved(e);
		const uint64_t total_ns = resolved ? (e.end_ns - e.start_ns) : 0;

		uint64_t children_ns = 0;
		for (uint32_t k = child_start; k < subtree_end;) {
			if (fe.events[k].depth == depth + 1) {
				if (is_resolved(fe.events[k]))
					children_ns += fe.events[k].end_ns - fe.events[k].start_ns;
				uint32_t k2 = k + 1;
				while (k2 < subtree_end && fe.events[k2].depth > depth + 1)
					k2++;
				k = k2;
			} else {
				k++;
			}
		}

		const bool has_children = subtree_end > child_start;
		const auto& loc			= ProfilerRegistry::location(e.slot);
		const double total_ms	= ns_to_ms(total_ns);
		const double self_ms	= ns_to_ms(total_ns > children_ns ? total_ns - children_ns : 0);
		const double pct		= frame_total_ns > 0 ? (100.0 * total_ns / frame_total_ns) : 0.0;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen;
		if (!has_children)
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		const bool open = ImGui::TreeNodeEx((void*)(intptr_t)node_index, flags, "%s", loc.name.c_str());
		ImGui::TableNextColumn();
		ImGui::Text("%.1f%%", pct);
		ImGui::TableNextColumn();
		if (resolved)
			ImGui::Text("%.3f", total_ms);
		else
			ImGui::TextUnformatted("-");
		ImGui::TableNextColumn();
		if (resolved)
			ImGui::Text("%.3f", self_ms);
		else
			ImGui::TextUnformatted("-");

		if (has_children && open) {
			draw_zone_rows(fe, child_start, subtree_end, depth + 1, frame_total_ns);
			ImGui::TreePop();
		}
		i = subtree_end;
	}
	return i;
}

void draw_zone_table(const char* table_id, const char* time_col_label, const FrameEvents* fe) {
	if (!fe || fe->events.empty()) {
		ImGui::TextDisabled("(no data for this frame)");
		return;
	}
	uint64_t lo = UINT64_MAX, hi = 0;
	for (auto& e : fe->events) {
		if (!is_resolved(e))
			continue;
		lo = std::min(lo, e.start_ns);
		hi = std::max(hi, e.end_ns);
	}
	const double frame_total_ns = (lo <= hi) ? double(hi - lo) : 0.0;

	if (ImGui::BeginTable(table_id, 4,
						   ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
		ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Total %", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn(time_col_label, ImGuiTableColumnFlags_WidthFixed, 80);
		ImGui::TableSetupColumn("Self ms", ImGuiTableColumnFlags_WidthFixed, 80);
		ImGui::TableHeadersRow();

		draw_zone_rows(*fe, 0, (uint32_t)fe->events.size(), 0, frame_total_ns);

		ImGui::EndTable();
	}
}

// ---- Tabs ------------------------------------------------------------

void draw_overall_tab() {
	ThreadCapture* main_tc = nullptr;
	for (auto* t : Profiler::all_threads())
		if (t->name() == "Main")
			main_tc = t;
	if (!main_tc || main_tc->ring.empty()) {
		ImGui::TextDisabled("no data yet");
		return;
	}
	ThreadCapture& gpu_tc = Profiler::gpu_capture();

	const size_t capacity  = main_tc->ring.size();
	const uint64_t upto	   = Profiler::last_committed_frame() + 1;
	const auto frames	   = display_window(capacity, upto);
	if (g_follow_live && upto > 0)
		g_scrub_frame = upto - 1;

	draw_sparkline("FPS", frames, [&](uint64_t f) {
		const double span = frame_span_ms(find_frame(main_tc->ring, f));
		return span > 0.0 ? 1000.0 / span : 0.0;
	});
	draw_sparkline("Update Time (ms)", frames,
					[&](uint64_t f) { return find_zone_ms(find_frame(main_tc->ring, f), "game_update_tick"); });
	draw_sparkline("Draw Time CPU (ms)", frames,
					[&](uint64_t f) { return find_zone_ms(find_frame(main_tc->ring, f), "scene_draw"); });
	draw_sparkline("Draw Time GPU (ms)", frames,
					[&](uint64_t f) { return find_zone_ms(find_frame(gpu_tc.ring, f), "scene_draw"); });
}

void draw_cpu_tab() {
	auto threads = Profiler::all_threads();
	if (threads.empty()) {
		ImGui::TextDisabled("no data yet");
		return;
	}
	ThreadCapture* main_tc = threads[0];
	for (auto* t : threads)
		if (t->name() == "Main")
			main_tc = t;
	if (main_tc->ring.empty()) {
		ImGui::TextDisabled("no data yet");
		return;
	}

	const size_t capacity = main_tc->ring.size();
	const uint64_t upto   = Profiler::last_committed_frame() + 1;
	const auto frames	   = display_window(capacity, upto);
	if (g_follow_live && upto > 0)
		g_scrub_frame = upto - 1;

	draw_sparkline("Frame Time (ms)", frames,
					[&](uint64_t f) { return frame_span_ms(find_frame(main_tc->ring, f)); });

	ImGui::Separator();
	ImGui::TextDisabled("Frame %llu", (unsigned long long)g_scrub_frame);

	ImGui::BeginChild("cpu_flame", ImVec2(0, 260), true);
	for (auto* t : threads)
		draw_flame_lane(t->name().c_str(), find_frame(t->ring, g_scrub_frame));
	ImGui::EndChild();

	ImGui::BeginChild("cpu_table", ImVec2(0, 0), true);
	draw_zone_table("cpu_table_grid", "Total ms", find_frame(main_tc->ring, g_scrub_frame));
	ImGui::EndChild();
}

void draw_gpu_tab() {
	ThreadCapture& gpu_tc = Profiler::gpu_capture();
	if (gpu_tc.ring.empty()) {
		ImGui::TextDisabled("no data yet (GPU results resolve a few frames after submission)");
		return;
	}

	const size_t capacity = gpu_tc.ring.size();
	const uint64_t upto   = Profiler::last_committed_frame() + 1;
	const auto frames	   = display_window(capacity, upto);
	if (g_follow_live && upto > 0)
		g_scrub_frame = upto - 1;

	draw_sparkline("GPU Time (ms)", frames, [&](uint64_t f) { return frame_span_ms(find_frame(gpu_tc.ring, f)); });

	ImGui::Separator();
	ImGui::TextDisabled("Frame %llu", (unsigned long long)g_scrub_frame);

	ImGui::BeginChild("gpu_flame", ImVec2(0, 260), true);
	draw_flame_lane("GPU", find_frame(gpu_tc.ring, g_scrub_frame));
	ImGui::EndChild();

	ImGui::BeginChild("gpu_table", ImVec2(0, 0), true);
	draw_zone_table("gpu_table_grid", "GPU ms", find_frame(gpu_tc.ring, g_scrub_frame));
	ImGui::EndChild();
}

void draw_profiler_window(bool* open) {
	ImGui::SetNextWindowSize(ImVec2(720, 560), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Profiler", open)) {
		ImGui::End();
		return;
	}

	draw_toolbar();
	ImGui::Separator();

	ImGui::BeginChild("prof_sidebar", ImVec2(120, 0), true);
	if (ImGui::Selectable("Overall", g_tab == ProfTab::Overall))
		g_tab = ProfTab::Overall;
	if (ImGui::Selectable("CPU", g_tab == ProfTab::Cpu))
		g_tab = ProfTab::Cpu;
	if (ImGui::Selectable("GPU", g_tab == ProfTab::Gpu))
		g_tab = ProfTab::Gpu;
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("prof_content", ImVec2(0, 0));
	switch (g_tab) {
	case ProfTab::Overall: draw_overall_tab(); break;
	case ProfTab::Cpu: draw_cpu_tab(); break;
	case ProfTab::Gpu: draw_gpu_tab(); break;
	}
	ImGui::EndChild();

	ImGui::End();
}

} // namespace

namespace prof_ui {
void draw() {
	if (!stat_profiler.get_bool())
		return;
	bool open = true;
	draw_profiler_window(&open);
	if (!open)
		stat_profiler.set_bool(false);
}
} // namespace prof_ui
