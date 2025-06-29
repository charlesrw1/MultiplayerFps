#pragma once
#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"
#include "Framework/StructReflection.h"

struct MyTestStruct
{
	STRUCT_BODY();
	REF std::string str;
	REF int integer = 0;
	REF bool cond = false;
};
class TestClass : public ClassBase
{
public:
	CLASS_BODY(TestClass);

	REF MyTestStruct structure;
	REF int someint = 0;
};