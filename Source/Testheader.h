#pragma once
#include <string>
#include "Framework/ClassBase.h"

class StaticClass : public ClassBase {
public:
	CLASS_BODY(StaticClass);
	REF static void do_something() {
		printf("doing something\n");
	}
};
//
class InterfaceClass : public ClassBase{
public:
	CLASS_BODY(InterfaceClass, scriptable);

	REF virtual int get_value(std::string str) { 
		printf("base get_value\n");

		return 0; 
	}
	REF virtual void buzzer() { 
		printf("base buzzer\n");
	}
	REF int self_func() {
		printf("self_func\n");

		return variable;
	}
	REF void set_var(int v) {
		printf("set_var %d\n",v);

		variable = v;
	}
	REF void set_str(string s) {
		myStr = s;
	}

	std::string myStr;
	int variable = 0;
};