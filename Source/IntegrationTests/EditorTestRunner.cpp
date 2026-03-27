// Source/IntegrationTests/EditorTestRunner.cpp
// Stub — full implementation in Task 8.
#include "EditorTestRunner.h"
#include <cstdio>

EditorTestRunner::EditorTestRunner(std::vector<TestEntry> tests, const TestRunnerConfig& cfg)
	: tests_(std::move(tests)), cfg_(cfg) {}

bool EditorTestRunner::tick(float /*dt*/) {
	printf("[EditorTestRunner] stub — no editor tests implemented yet\n");
	return true;
}
