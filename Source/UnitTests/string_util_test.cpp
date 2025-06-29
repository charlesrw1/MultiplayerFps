#include "Unittest.h"
#include "Framework/StringUtils.h"

ADD_TEST(string_utils)
{
	checkTrue(StringUtils::get_extension("myfile/something.xyz ") == ".xyz");
	checkTrue(StringUtils::get_extension(" myfile\\something.xyz ") == ".xyz");
	checkTrue(StringUtils::strip("\r  \tsome string\t   \r") == "some string");
	std::string str = "thing;with;things";
	StringUtils::replace(str, ";", " ; ");
	checkTrue(str == "thing ; with ; things");
	checkTrue(StringUtils::starts_with("abc", "ab"));

	auto tokens = StringUtils::split(" \tthis is\ta token ");
	checkTrue(tokens.size() == 4 && tokens[0] == "this" && tokens[1] == "is" && tokens[2] == "a" && tokens[3] == "token");

}