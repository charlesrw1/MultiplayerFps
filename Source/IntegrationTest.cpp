#include "IntegrationTest.h"

IntegrationTester::IntegrationTester(bool exit_on_finish, vector<IntTestCase>& test_cases)
	: is_done(false), can_do_work(false)
{
	testcases = std::move(test_cases);
	thread = std::thread(&IntegrationTester::tester_thread, this);
	total_tests = testcases.size();
}

// called in main thread

bool IntegrationTester::tick(float dt) {
	if (waiting_time > 0)
		waiting_time -= dt;
	if (waiting_time > 0.0)
		return false;
	if (waiting_ticks > 0)
		waiting_ticks--;
	if (waiting_ticks > 0)
		return false;
	if (waiting_on_delegate) {
		time_waiting_on_delegate += dt;
		if (time_waiting_on_delegate > delgate_timeout_max) {
			waiting_on_delegate = false;
			abort_because_waited_too_long = true;
			time_waiting_on_delegate = 0.0;
		}
		else {
			return false;
		}
	}
	else {
		time_waiting_on_delegate = 0.0;
	}

	can_do_work = true;
	cv.notify_all();
	std::unique_lock<std::mutex> l(mutex);
	cv.wait(l, [&]() { return is_done || !can_do_work; });

	if (is_done) {
		sys_print(Info, "Integration tests finished. (%d/%d passed)\n", total_tests-failed_tests,total_tests);
	}

	return is_done;
}

// called by test functions

void IntegrationTester::wait_time(float time) {
	waiting_time = time;
	can_do_work = false;
	cv.notify_one();
	std::unique_lock<std::mutex> l(mutex);
	cv.wait(l, [&]() { return can_do_work; });
}

void IntegrationTester::wait_ticks(int ticks) {
	waiting_ticks = ticks;
	can_do_work = false;
	cv.notify_one();
	std::unique_lock<std::mutex> l(mutex);
	cv.wait(l, [&]() { return can_do_work; });
}

void IntegrationTester::wait_delegate_shared() {
	can_do_work = false;
	cv.notify_one();
	std::unique_lock<std::mutex> l(mutex);
	cv.wait(l, [&]() { return can_do_work; });
	assert(!waiting_on_delegate);
	if (abort_because_waited_too_long) {
		abort_because_waited_too_long = false;
		throw std::runtime_error("Delegate wait took too long");
	}
}
void IntegrationTester::wait_delegate(MulticastDelegate<>& delegate) {
	assert(!waiting_on_delegate);
	waiting_on_delegate = true;
	delegate.add(this, [this, &delegate]() {
		waiting_on_delegate = false;
		delegate.remove(this);
		// resume work, like a tick, blocks main thread since delegate is called on main thread
		tick(0.0f);
	});
	wait_delegate_shared();
	delegate.remove(this);
}

void IntegrationTester::checkTrue(bool b, const char* msg) {
	if (!b)
		throw std::runtime_error(msg);	// caught by tester_thread
}

IntegrationTester::~IntegrationTester() {
	if (thread.joinable())
		thread.join();
}

void IntegrationTester::tester_thread() {
	try {

		while (!testcases.empty()) {
			IntTestCase test = std::move(testcases.back());
			testcases.pop_back();
			try {
				test.func(*this);
				// Test passed
				sys_print(Info, "Test '%s' passed: %s\n", test.test_name.c_str());
			}
			catch (const std::exception& e) {
				sys_print(Error, "Test '%s' failed: %s\n", test.test_name.c_str(), e.what());
				failed_tests++;
			}
			catch (...) {
				sys_print(Error, "Test '%s' failed: unknown exception\n", test.test_name.c_str());
				failed_tests++;
			}
			// After each test, yield to main thread
			{
				std::lock_guard<std::mutex> l(mutex);
				can_do_work = false;
				is_done = testcases.empty();
			}
			cv.notify_all();
			if (!is_done) {
				// Wait for main thread to allow next test
				std::unique_lock<std::mutex> l(mutex);
				cv.wait(l, [&]() { return can_do_work; });
			}
		}
		{
			std::lock_guard<std::mutex> l(mutex);
			is_done = true;
		}
		cv.notify_all();
	}
	catch (...) {
		std::lock_guard<std::mutex> l(mutex);
		is_done = true;
		cv.notify_all();
		throw;
	}
}
