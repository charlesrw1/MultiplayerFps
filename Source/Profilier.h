#pragma once
#ifndef PROFILING_H
#define PROFILING_H

class Profiler
{
public:
	static Profiler* get_instance();

	virtual void start_scope(const char* name, bool include_gpu) = 0;
	virtual void end_scope(const char* name) = 0;
	virtual void end_frame_tick() = 0;
};

struct Profile_Scope_Wrapper
{
	Profile_Scope_Wrapper(const char* name, bool gpu) :myname(name) {
		Profiler::get_instance()->start_scope(name, gpu);
	}
	~Profile_Scope_Wrapper() {
		Profiler::get_instance()->end_scope(myname);
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
