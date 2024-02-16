#pragma once
#ifndef PROFILING_H
#define PROFILING_H
#include "Util.h"
#include <vector>

struct Profile_Event
{
	const char* name="";
	bool enabled = false;
	uint64_t last_interval_time_cpu = 0;
	uint64_t cpustart = 0;
	uint64_t cputime = 0;	// microseconds
	uint32_t accumulated_cpu = 0;
	bool started = false;

	uint64_t last_interval_time_gpu = 0;
	uint64_t gputime = 0;	// nanoseconds
	uint32_t glquery[2];
	uint32_t accumulated_gpu = 0;
	bool waiting = false;

	bool is_gpu_event = false;
	int parent_event = -1;
};


class Profilier
{
public:
	Profilier();
	static Profilier& get_instance();

	void start_scope(const char* name, bool include_gpu);
	void end_scope(const char* name);

	void end_frame_tick();

	void draw_imgui_window();

	std::vector<Profile_Event> events;
	std::vector<int> stack;

private:
	uint64_t intervalstart = 0;
	int accumulated = 1;
};

struct Profile_Scope_Wrapper
{
	Profile_Scope_Wrapper(const char* name, bool gpu) :myname(name) {
		Profilier::get_instance().start_scope(name, gpu);
	}
	~Profile_Scope_Wrapper() {
		Profilier::get_instance().end_scope(myname);
	}

	const char* myname;
};
#define PROFILING

#ifdef PROFILING
#define MAKEPROF_(type, include_gpu) Profile_Scope_Wrapper PROFILEEVENT##__LINE__(type, include_gpu)
#define CPUFUNCTIONSTART MAKEPROF_(__FUNCTION__, false)
#define GPUFUNCTIONSTART MAKEPROF_(__FUNCTION__, true)
#define GPUSCOPESTART(name) MAKEPROF_(name, true)
#define CPUSCOPESTART(name) MAKEPROF_(name, false)


#else
#define CPUPROF(type)
#define GPUPROF(type)
#endif




#endif // !PROFILING_H
