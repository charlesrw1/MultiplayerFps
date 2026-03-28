// Source/IntegrationTests/GameTestRunner.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "GameTestRunner.h"
#include "GpuTimer.h"
#include <cstdio>
#include <ctime>
#include <direct.h>
#include "GameEnginePublic.h"
#include "Framework/StringUtils.h"
#include "StateDump.h"
#include "LuaDebugServer.h"
#include "Scripting/ScriptManager.h"
GameTestRunner::GameTestRunner(std::string_view name, std::vector<TestEntry> tests, const TestRunnerConfig& cfg)
	: tests_(std::move(tests)), cfg_(cfg), name(name) {
	screenshot_cfg_.promote = cfg.promote;
	screenshot_cfg_.interactive = cfg.interactive;
	is_this_editor_mode = name == "Editor";

	ctx_.is_editor_mode = is_this_editor_mode;
}

bool GameTestRunner::tick(float dt) {
	// First call: kick off the first test

	const std::string xml_path = "TestFiles/integration_" + StringUtils::to_lower(name.data()) + "_results.xml";

	if (current_idx_ < 0) {
		start_next_test();
		if (current_idx_ >= (int)tests_.size()) {
			write_results_xml(xml_path.c_str());
			return true;
		}
		return false;
	}

	if (current_idx_ >= (int)tests_.size())
		return true;

	auto& entry = tests_[current_idx_];
	elapsed_ += dt;

	// Timeout guard
	if (elapsed_ > entry.timeout_seconds) {
		fprintf(stderr, "[TIMEOUT] %s (%.1fs)\n", entry.name, elapsed_);
		finish_current_test("TIMEOUT");
		start_next_test();
		if (current_idx_ >= (int)tests_.size()) {
			write_results_xml(xml_path.c_str());
			return true;
		}
		return false;
	}

	// Debug break: AI file-based Lua REPL (co_await t.debug_break())
	if (ctx_.wait.debug_break_pending) {
		if (!debug_break_entered_) {
			debug_break_entered_ = true;
			LuaDebugServer::on_enter(tests_[current_idx_].name);
		}
		if (LuaDebugServer::poll(ScriptManager::inst->get_lua_state())) {
			ctx_.wait.debug_break_pending = false;
			debug_break_entered_ = false;
		}
		return false;
	}

	// State dump: requested via co_await t.dump_state("label")
	if (ctx_.wait.dump_state_pending) {
		ctx_.wait.dump_state_pending = false;
		std::string full_label = std::string(tests_[current_idx_].name) + "_" + ctx_.wait.dump_state_label;
		// sanitize label for use as filename
		for (char& c : full_label)
			if (c == '/' || c == '\\' || c == ':' || c == ' ')
				c = '_';
		print_engine_state(full_label.c_str());
	}

	// Screenshot: capture after the frame that rendered (wait_ticks reached 0)
	if (ctx_.wait.screenshot_pending && ctx_.wait.wait_ticks == 0) {
		ctx_.wait.screenshot_pending = false;
		bool ok =
			screenshot_capture_and_compare(ctx_.wait.screenshot_name.c_str(), screenshot_cfg_, get_app_window_size());
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
	if (task_ && !task_->done()) {
		task_->resume();
	}

	// Test finished?
	if (!task_ || task_->done()) {
		finish_current_test(nullptr);
		start_next_test();
		if (current_idx_ >= (int)tests_.size()) {
			write_results_xml(xml_path.c_str());
			return true;
		}
	}

	return false;
}

void GameTestRunner::start_next_test() {
	++current_idx_;
	if (current_idx_ >= (int)tests_.size())
		return;
	auto& entry = tests_[current_idx_];
	printf("\n==> [%d/%d] %s\n", current_idx_ + 1, (int)tests_.size(), entry.name);
	elapsed_ = 0.f;
	ctx_ = TestContext{};
	debug_break_entered_ = false;
	ctx_.is_editor_mode = is_this_editor_mode;
	task_.emplace(entry.fn(ctx_));
}

void GameTestRunner::finish_current_test(const char* reason) {
	auto& entry = tests_[current_idx_];
	bool passed = ctx_.checks_failed == 0 && reason == nullptr;
	if (reason)
		ctx_.failures.push_back(reason);
	if (passed) {
		printf("  PASS  %s  (%d checks)\n", entry.name, ctx_.checks_passed);
	} else {
		fprintf(stderr, "  FAIL  %s\n", entry.name);
		for (auto& f : ctx_.failures)
			fprintf(stderr, "    %s\n", f.c_str());
	}
	results_.push_back({entry.name, passed, ctx_.failures});
	if (passed)
		++passed_count_;
	else
		++failed_count_;
}

void GameTestRunner::write_results_xml(const char* path) {
	printf("\n=== %s Tests: %d passed, %d failed ===\n", name.data(), passed_count_, failed_count_);
	_mkdir("TestFiles");
	FILE* f = fopen(path, "w");
	if (!f)
		return;
	fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(f, "<testsuite name=\"IntegrationTests.%s\" tests=\"%d\" failures=\"%d\">\n", name.data(),
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
