// Source/IntegrationTests/TestRunner.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "TestRunner.h"
#include "GpuTimer.h"
#include <cstdio>
#include <ctime>
#include <direct.h>
#include "GameEnginePublic.h"
#include "Scripting/ScriptManager.h"
#include "Framework/SysPrint.h"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

static const char* mode_name(TestMode m) {
	return m == TestMode::Editor ? "editor" : "game";
}

TestRunner::TestRunner(TestMode mode, std::vector<TestEntry> tests, const TestRunnerConfig& cfg)
	: tests_(std::move(tests)), cfg_(cfg), mode_(mode), lua_co_ref_(LUA_NOREF) {
	screenshot_cfg_.promote = cfg.promote;
	screenshot_cfg_.interactive = cfg.interactive;
	ctx_.is_editor_mode = (mode == TestMode::Editor);
	LuaTestRunner::active_sink = this;
}

TestRunner::~TestRunner() {
	if (LuaTestRunner::active_sink == this)
		LuaTestRunner::active_sink = nullptr;
	if (lua_co_ref_ != LUA_NOREF && ScriptManager::inst) {
		luaL_unref(ScriptManager::inst->get_lua_state(), LUA_REGISTRYINDEX, lua_co_ref_);
		lua_co_ref_ = LUA_NOREF;
	}
}

bool TestRunner::tick(float dt) {
	const std::string xml_path = std::string("TestFiles/integration_") + mode_name(mode_) + "_results.xml";

	if (phase_ == Phase::Cpp) {
		if (current_idx_ < 0) {
			start_next_test();
			if (current_idx_ >= (int)tests_.size()) {
				start_lua_phase();
				if (phase_ == Phase::Done) {
					write_results_xml(xml_path.c_str());
					return true;
				}
				return false;
			}
			return false;
		}

		auto& entry = tests_[current_idx_];
		elapsed_ += dt;

		// Timeout guard
		if (elapsed_ > entry.timeout_seconds) {
			fprintf(stderr, "[TEST] [TIMEOUT] %s (%.1fs)\n", entry.name, elapsed_);
			finish_current_test("TIMEOUT");
			start_next_test();
			if (current_idx_ >= (int)tests_.size()) {
				start_lua_phase();
				if (phase_ == Phase::Done) {
					write_results_xml(xml_path.c_str());
					return true;
				}
			}
			return false;
		}

		// Screenshot: capture after the frame that rendered (wait_ticks reached 0)
		if (ctx_.wait.screenshot_pending && ctx_.wait.wait_ticks == 0) {
			ctx_.wait.screenshot_pending = false;
			ScreenshotConfig cfg = screenshot_cfg_;
			if (ctx_.wait.screenshot_warn_channel_delta >= 0)
				cfg.warn_channel_delta = ctx_.wait.screenshot_warn_channel_delta;
			if (ctx_.wait.screenshot_warn_diff_fraction >= 0.f)
				cfg.warn_diff_fraction = ctx_.wait.screenshot_warn_diff_fraction;
			bool ok = screenshot_capture_and_compare(ctx_.wait.screenshot_name.c_str(), cfg,
													 get_app_window_size());
			if (!ok)
				ctx_.check(false, ("screenshot failed: " + ctx_.wait.screenshot_name).c_str());
		}

		// Yield conditions — return false to keep waiting
		if (ctx_.wait.wait_ticks > 0) {
			--ctx_.wait.wait_ticks;
			return false;
		}
		if (ctx_.wait.wait_seconds > 0.f) {
			ctx_.wait.wait_seconds -= dt;
			return false;
		}
		if (ctx_.wait.waiting_delegate) {
			if (!ctx_.wait.delegate_fired)
				return false;
			ctx_.wait.waiting_delegate = false;
			ctx_.wait.delegate_fired = false;
		}

		// Resume coroutine
		if (task_ && !task_->done())
			task_->resume();

		// Test finished?
		if (!task_ || task_->done()) {
			finish_current_test(nullptr);
			start_next_test();
			if (current_idx_ >= (int)tests_.size()) {
				start_lua_phase();
				if (phase_ == Phase::Done) {
					write_results_xml(xml_path.c_str());
					return true;
				}
			}
		}
		return false;
	}

	if (phase_ == Phase::Lua) {
		if (tick_lua(dt)) {
			phase_ = Phase::Done;
			write_results_xml(xml_path.c_str());
			return true;
		}
		return false;
	}

	return true;
}

void TestRunner::start_next_test() {
	++current_idx_;
	if (current_idx_ >= (int)tests_.size())
		return;
	auto& entry = tests_[current_idx_];
	printf("\n[TEST] ==> [%d/%d] %s\n", current_idx_ + 1, (int)tests_.size(), entry.name);
	elapsed_ = 0.f;
	ctx_ = TestContext{};
	ctx_.is_editor_mode = (mode_ == TestMode::Editor);
	task_.emplace(entry.fn(ctx_));
}

void TestRunner::finish_current_test(const char* reason) {
	auto& entry = tests_[current_idx_];
	bool passed = ctx_.checks_failed == 0 && reason == nullptr;
	if (reason)
		ctx_.failures.push_back(reason);
	if (passed) {
		printf("[TEST]   PASS  %s  (%d checks)\n", entry.name, ctx_.checks_passed);
	} else {
		fprintf(stderr, "[TEST]   FAIL  %s\n", entry.name);
		for (auto& f : ctx_.failures)
			fprintf(stderr, "[TEST]     %s\n", f.c_str());
	}
	results_.push_back({entry.name, passed, ctx_.failures});
	if (passed)
		++passed_count_;
	else
		++failed_count_;
}

void TestRunner::write_results_xml(const char* path) {
	printf("\n[TEST] === %s Tests: %d passed, %d failed ===\n", mode_name(mode_), passed_count_, failed_count_);
	_mkdir("TestFiles");
	FILE* f = fopen(path, "w");
	if (!f)
		return;
	fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(f, "<testsuite name=\"IntegrationTests.%s\" tests=\"%d\" failures=\"%d\">\n", mode_name(mode_),
			(int)results_.size(), failed_count_);
	for (auto& r : results_) {
		fprintf(f, "  <testcase name=\"%s\">\n", r.name.c_str());
		for (auto& fail : r.failures)
			fprintf(f, "    <failure message=\"%s\"/>\n", fail.c_str());
		fprintf(f, "  </testcase>\n");
	}
	fprintf(f, "</testsuite>\n");
	fclose(f);

	time_t t = time(nullptr);
	char date[16];
	strftime(date, sizeof(date), "%Y-%m-%d", localtime(&t));
	std::string timing_path = std::string("TestFiles/timing_") + date + ".json";
	GpuTimingLog::get().write_json(timing_path.c_str());
}

// ---- Lua phase ------------------------------------------------------------

void TestRunner::start_lua_phase() {
	if (!ScriptManager::inst) {
		phase_ = Phase::Done;
		return;
	}
	lua_State* L = ScriptManager::inst->get_lua_state();

	// _lua_run_all_tests is defined by Data/scripts/integration_test_framework.lua;
	// no Lua test framework loaded => skip the phase.
	lua_getglobal(L, "_lua_run_all_tests");
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1);
		phase_ = Phase::Done;
		return;
	}

	// Push test pattern table for filtering: _test_patterns = { "p1", "p2", ... }
	lua_createtable(L, (int)lua_patterns_.size(), 0);
	for (size_t i = 0; i < lua_patterns_.size(); ++i) {
		lua_pushstring(L, lua_patterns_[i].c_str());
		lua_rawseti(L, -2, (int)i + 1);
	}
	lua_setglobal(L, "_test_patterns");

	// Create coroutine wrapping _lua_run_all_tests and stash it in registry.
	lua_State* co = lua_newthread(L); // pushes thread
	lua_co_ref_ = luaL_ref(L, LUA_REGISTRYINDEX);
	(void)co;
	// Push the function into the coroutine's stack so subsequent lua_resume runs it.
	lua_rawgeti(L, LUA_REGISTRYINDEX, lua_co_ref_);
	lua_State* co2 = lua_tothread(L, -1);
	lua_pop(L, 1);
	lua_getglobal(co2, "_lua_run_all_tests");
	if (!lua_isfunction(co2, -1)) {
		lua_pop(co2, 1);
		luaL_unref(L, LUA_REGISTRYINDEX, lua_co_ref_);
		lua_co_ref_ = LUA_NOREF;
		phase_ = Phase::Done;
		return;
	}

	phase_ = Phase::Lua;
	lua_wait_ = 0.f;
	lua_done_ = false;
	printf("\n[TEST] === Lua Tests ===\n");
}

bool TestRunner::tick_lua(float dt) {
	if (lua_done_)
		return true;
	if (lua_co_ref_ == LUA_NOREF || !ScriptManager::inst)
		return true;

	lua_wait_ -= dt;
	if (lua_wait_ > 0.f)
		return false;
	lua_wait_ = 0.f;

	lua_State* L = ScriptManager::inst->get_lua_state();
	lua_rawgeti(L, LUA_REGISTRYINDEX, lua_co_ref_);
	lua_State* co = lua_tothread(L, -1);
	lua_pop(L, 1);
	if (!co) {
		lua_done_ = true;
		return true;
	}

	int nresults = 0;
	int status = lua_resume(co, nullptr, 0, &nresults);
	if (status == LUA_OK) {
		// coroutine returned normally — treat as done
		lua_pop(co, nresults);
		lua_done_ = true;
		return true;
	}
	if (status == LUA_YIELD) {
		float wait_secs = 0.f;
		if (nresults > 0 && lua_isnumber(co, -nresults))
			wait_secs = (float)lua_tonumber(co, -nresults);
		lua_pop(co, nresults);
		lua_wait_ = wait_secs;
		return lua_done_; // set_done() may have fired during this resume
	}
	// error
	const char* msg = lua_tostring(co, -1);
	fprintf(stderr, "[TEST] [Lua test runner error] %s\n", msg ? msg : "(no message)");
	results_.push_back({"_lua_runner", false, {msg ? msg : "lua error"}});
	++failed_count_;
	lua_done_ = true;
	return true;
}

void TestRunner::report(const std::string& name, bool passed, const std::string& message) {
	std::vector<std::string> failures;
	if (!passed)
		failures.push_back(message);
	if (passed) {
		printf("[TEST]   PASS  %s\n", name.c_str());
		++passed_count_;
	} else {
		fprintf(stderr, "[TEST]   FAIL  %s\n[TEST]     %s\n", name.c_str(), message.c_str());
		++failed_count_;
	}
	results_.push_back({name, passed, std::move(failures)});
}

void TestRunner::set_done() {
	lua_done_ = true;
}
