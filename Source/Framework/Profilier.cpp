#include "Framework/Util.h"
#include "glad/glad.h"
#include "imgui.h"
#include <chrono>
#include <SDL2/SDL.h>
#include <vector>
#include "Config.h"

using std::chrono::steady_clock;
using std::chrono::microseconds;


struct Profile_Event
{
	const char* name = "";
	bool enabled = false;
	uint64_t last_interval_time_cpu = 0;
	uint64_t cpustart = 0;
	uint64_t cputime = 0;	// microseconds
	uint32_t accumulated_cpu = 0;

	uint64_t last_period_high = 0;
	uint64_t period_high = 0;


	bool started = false;

	uint64_t last_interval_time_gpu = 0;
	uint64_t gputime = 0;	// nanoseconds
	uint32_t glquery[2];
	uint32_t accumulated_gpu = 0;
	bool waiting = false;

	bool is_gpu_event = false;
	int parent_event = -1;
};

std::vector<Profile_Event> events;
std::vector<int> stack;
uint64_t intervalstart = 0;
int accumulated = 1;
struct FrameHighStruct
{
	float accumulator = 0.0;
	float high_this_period = 0.0;
	float high_last_period = 0.0;
	int tick_count = 0;
	int tick_count_last_period = 0;

	int ticks_above_average_epsilon_last = 0;

	int ticks_above_average_epsilon = 0;


	void update(float dt, float interval) {
		accumulator += dt;
		tick_count++;
		if (accumulator >= interval) {
			high_last_period = high_this_period;
			high_this_period = 0.0;
			accumulator = 0.0;
			tick_count_last_period = tick_count;
			tick_count = 0;

			ticks_above_average_epsilon_last = ticks_above_average_epsilon;
			ticks_above_average_epsilon = 0;
		}
		high_this_period = std::max(high_this_period, dt);
		const float avg_last = interval / tick_count_last_period;
		if (dt >= avg_last * 1.3) {
			ticks_above_average_epsilon++;
		}
	}
};
static FrameHighStruct one_second;
static FrameHighStruct ten_second;



// super not optimized lol
static void draw_node_children(int index)
{
	Profile_Event& e = events[index];
	if (ImGui::TreeNodeEx(e.name, ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("CPU avg: %3.5f\n", e.last_interval_time_cpu / 1000.0);
		ImGui::Text("CPU max: %3.5f\n", e.last_period_high / 1000.0);
		if (e.is_gpu_event) {
			ImGui::Text("GPU: %3.5f\n", e.last_interval_time_gpu / 1000000.0);
		}

		for (int i = 0; i < events.size(); i++) {
			if (events[i].parent_event == index) {
				draw_node_children(i);
			}
		}

		ImGui::TreePop();
	}
}

static void draw_imgui_profile_window()
{

	{
		ImGui::Text("1s high: %f", one_second.high_last_period);
		ImGui::Text("1s avg: %f", 1.0/one_second.tick_count_last_period);
		ImGui::Text("1s percent above middle: %f", one_second.ticks_above_average_epsilon_last / (float)one_second.tick_count_last_period);
		ImGui::Text("10s high: %f", ten_second.high_last_period);
		ImGui::Text("10s avg: %f", 10.0/ten_second.tick_count_last_period);

		for (int i = 0; i < events.size(); i++) {
			if (events[i].parent_event == -1) {
				draw_node_children(i);
			}
		}
	}
}



void Profiler::init()
{
	intervalstart = SDL_GetPerformanceCounter();

	Debug_Interface::get()->add_hook("Profiling", draw_imgui_profile_window);

}

void Profiler::end_frame_tick(float dt)
{
	uint64_t timenow = SDL_GetPerformanceCounter();

	one_second.update(dt, 1.0);
	ten_second.update(dt, 10.0);


	if ((timenow - intervalstart) / (double)SDL_GetPerformanceFrequency() > 1.0) {
		
		accumulated = 0;
		for (int i = 0; i < events.size(); i++) {
			Profile_Event& e = events[i];
			e.last_interval_time_cpu = (e.accumulated_cpu > 0) ? e.cputime / e.accumulated_cpu : 0;
			e.cputime = 0;
			e.accumulated_cpu = 0;

			e.last_period_high = e.period_high;
			e.period_high = 0;

			if (e.is_gpu_event) {
				e.last_interval_time_gpu = (e.accumulated_gpu > 0) ? e.gputime / e.accumulated_gpu : 0;
				e.gputime = 0;
				e.accumulated_gpu = 0;
			}
		}

		intervalstart = timenow;
	}
}


void Profiler::start_scope(const char* name, bool gpu)
{
	int index = 0;
	for (; index < events.size(); index++) {
		if (strcmp(events[index].name, name) == 0)
			break;
	}
	if (index == events.size()) {
		events.push_back(Profile_Event());
		events[index].name = name;
		if (gpu) {
			events[index].is_gpu_event = true;
			glGenQueries(2, events[index].glquery);
		}
	}
	Profile_Event& e = events[index];
	if (stack.size() > 0)
		e.parent_event = stack.at(stack.size() - 1);
	else
		e.parent_event = -1;

	assert(!e.started);
	e.started = true;
	e.cpustart = std::chrono::duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();

	if (e.is_gpu_event && !e.waiting) {
		glQueryCounter(e.glquery[0], GL_TIMESTAMP);
	}

	if(e.is_gpu_event)
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);

	stack.push_back(index);

	glCheckError();
}

void Profiler::end_scope(const char* name)
{
	int index = 0;
	for (; index < events.size(); index++) {
		if (strcmp(events[index].name, name) == 0) break;
	}
	ASSERT(index != events.size());
	Profile_Event& e = events[index];
	ASSERT(e.started);
	e.started = false;
	uint64_t timenow = std::chrono::duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	const uint64_t dt = timenow - e.cpustart;
	e.cputime += dt;
	e.period_high = std::max(e.period_high, dt);

	e.accumulated_cpu++;

	if (e.is_gpu_event) {
		if (!e.waiting) {
			glQueryCounter(e.glquery[1], GL_TIMESTAMP);
			glCheckError();
			e.waiting = true;
		}
		int available{};
		glGetQueryObjectiv(e.glquery[1], GL_QUERY_RESULT_AVAILABLE, &available);
		glCheckError();

		if (available == GL_TRUE) {
			uint64_t starttime{}, stoptime{};

			glGetQueryObjectui64v(e.glquery[0], GL_QUERY_RESULT, &starttime);
			glGetQueryObjectui64v(e.glquery[1], GL_QUERY_RESULT, &stoptime);

			e.gputime += stoptime - starttime;
			e.accumulated_gpu++;
			//e.queryback = !e.queryback;
			e.waiting = false;
		}
	}

	if (e.is_gpu_event)
		glPopDebugGroup();

	stack.pop_back();
}