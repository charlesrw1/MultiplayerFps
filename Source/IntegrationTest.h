#pragma once
#include <functional>
#include <vector>
#include <string>
#include "Framework/MulticastDelegate.h"
using std::vector;
using std::string;
using std::function;
#include <thread>
#include <mutex>
#include <condition_variable>
#include "Framework/Util.h"
#include <cassert>

class IntegrationTester;
struct IntTestCase {
	function<void(IntegrationTester&)> func;
	string test_name;
	float time_limit = 1.0;
};

class IntegrationTester
{
public:
	IntegrationTester(bool exit_on_finish, vector<IntTestCase>& test_cases);
	// called in main thread
	bool tick(float dt);
	// called by test functions
	void wait_time(float time);
	void wait_ticks(int ticks);
	void wait_delegate(MulticastDelegate<>& delegate);
	template<typename T>
	T wait_delegate(MulticastDelegate<T>& delegate);
	void checkTrue(bool b, const char* msg);
	~IntegrationTester();
private:
	void wait_delegate_shared();
	void tester_thread();

	int failed_tests = 0;
	int total_tests = 0;

	vector<IntTestCase> testcases;
	bool abort_because_waited_too_long = false;
	float time_waiting_on_delegate = 0.0;
	float delgate_timeout_max = 2.0;
	bool waiting_on_delegate = false;
	int waiting_ticks = 0;
	float waiting_time = 0.0;
	bool can_do_work = false;
	bool is_done = false;
	std::mutex mutex;
	std::condition_variable cv;
	std::thread thread;
};

template<typename T>
inline T IntegrationTester::wait_delegate(MulticastDelegate<T>& delegate) {
	assert(!waiting_on_delegate);
	waiting_on_delegate = true;
	T outVal = T();
	delegate.add(this, [this, &delegate,&outVal](T dummy) {
		outVal = dummy;
		waiting_on_delegate = false;
		delegate.remove(this);
		// resume work, like a tick, blocks main thread since delegate is called on main thread
		tick(0.0f);
	});
	wait_delegate_shared();
	delegate.remove(this);
	return outVal;
}
