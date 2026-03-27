#pragma once

class ITestRunner
{
public:
	virtual ~ITestRunner() = default;
	// Called once per engine tick. Returns true when all tests are done.
	virtual bool tick(float dt) = 0;
	// 0 = all passed, 1 = one or more failures
	virtual int exit_code() const = 0;
};
