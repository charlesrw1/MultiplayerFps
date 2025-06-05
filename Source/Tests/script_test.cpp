#include "Unittest.h"
#include "Scripting/ScriptManager.h"
ADD_TEST(script_parse)
{
	const std::string& filename = "TestFiles/LuaParseTest.lua";
	auto text = UnitTestUtil::get_text_of_file(filename);

	auto result = ScriptLoadingUtil::parse_text(text);

	TEST_TRUE(result.size() == 4);
}