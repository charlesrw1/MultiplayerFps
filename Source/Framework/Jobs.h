#pragma once
#include "Framework/Config.h"
#include <future>
extern ConfigVar with_threading;

namespace job {

struct TaskRes
{
	TaskRes() {}
	TaskRes(std::future<void> future) : future(std::move(future)) {}
	void wait() {
		if (future.valid())
			future.wait();
	}
	std::future<void> future;
};

template<typename FUNC>
inline TaskRes launch_task(FUNC&& f)
{
	if (with_threading.get_bool()) {
		return std::async(std::launch::async, f);
	}
	else {
		f();
		return TaskRes();
	}
}

template<typename FUNC>
inline void launch_tasks(TaskRes* buf, int count, FUNC&& f)
{
	if (with_threading.get_bool()) {
		for (int i = 0; i < count; i++)
			buf[i] = std::async(std::launch::async, [f, i]()
				{
					f(i);
				});
	}
	else {
		for (int i = 0; i < count; i++)
			f(i);
	}
}
inline void wait_for_all(TaskRes* buf, int count)
{
	for (int i = 0; i < count; i++)
		buf[i].wait();
}
}
