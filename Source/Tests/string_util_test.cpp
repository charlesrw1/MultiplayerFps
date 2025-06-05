#include "Unittest.h"
#include "Framework/StringUtils.h"

ADD_TEST(string_utils)
{
	TEST_TRUE(StringUtils::get_extension("myfile/something.xyz ") == ".xyz");
	TEST_TRUE(StringUtils::get_extension(" myfile\\something.xyz ") == ".xyz");
	TEST_TRUE(StringUtils::strip("\r  \tsome string\t   \r") == "some string");
	std::string str = "thing;with;things";
	StringUtils::replace(str, ";", " ; ");
	TEST_TRUE(str == "thing ; with ; things");
	TEST_TRUE(StringUtils::starts_with("abc", "ab"));

	auto tokens = StringUtils::split(" \tthis is\ta token ");
	TEST_TRUE(tokens.size() == 4 && tokens[0] == "this" && tokens[1] == "is" && tokens[2] == "a" && tokens[3] == "token");

}