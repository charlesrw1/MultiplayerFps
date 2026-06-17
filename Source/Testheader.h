#pragma once
#include <string>
#include "Framework/ClassBase.h"
#include "Framework/InterfaceTypeInfo.h"
#include "Framework/MulticastDelegate.h"

class ITestInterface {
public:
	INTERFACE_BODY();
	REF virtual int interface_value() { return 42; }
	REF virtual void interface_action() {}
};

class ISecondInterface {
public:
	INTERFACE_BODY();
	REF virtual std::string second_name() { return "default"; }
};

class TestWithInterface : public ClassBase, public ITestInterface
{
public:
	CLASS_BODY(TestWithInterface, scriptable);
	int interface_value() override { return 100; }
	void interface_action() override {}
};

class TestWithTwoInterfaces : public ClassBase, public ITestInterface, public ISecondInterface
{
public:
	CLASS_BODY(TestWithTwoInterfaces);
	int interface_value() override { return 200; }
	void interface_action() override {}
	std::string second_name() override { return "two_ifaces"; }
};

class InterfaceClass : public ClassBase
{
public:
	CLASS_BODY(InterfaceClass, scriptable);

	REF virtual int get_value(std::string str) {
		printf("base get_value\n");

		return 0;
	}
	REF virtual void buzzer() { printf("base buzzer\n"); }
	REF int self_func() {
		printf("self_func\n");

		return variable;
	}
	REF void set_var(int v) {
		printf("set_var %d\n", v);

		variable = v;
	}
	REF void set_str(string s) { myStr = s; }

	std::string myStr;
	int variable = 0;
};