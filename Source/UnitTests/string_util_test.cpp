#include <gtest/gtest.h>
#include "Framework/StringUtils.h"

TEST(StringUtilsTest, GetExtension)
{
    EXPECT_EQ(StringUtils::get_extension("myfile/something.xyz "), ".xyz");
    EXPECT_EQ(StringUtils::get_extension(" myfile\\something.xyz "), ".xyz");
}

TEST(StringUtilsTest, Strip)
{
    EXPECT_EQ(StringUtils::strip("\r  \tsome string\t   \r"), "some string");
}

TEST(StringUtilsTest, Replace)
{
    std::string str = "thing;with;things";
    StringUtils::replace(str, ";", " ; ");
    EXPECT_EQ(str, "thing ; with ; things");
}

TEST(StringUtilsTest, StartsWith)
{
    EXPECT_TRUE(StringUtils::starts_with("abc", "ab"));
}

TEST(StringUtilsTest, Split)
{
    auto tokens = StringUtils::split(" \tthis is\ta token ");
    ASSERT_EQ(tokens.size(), 4u);
    EXPECT_EQ(tokens[0], "this");
    EXPECT_EQ(tokens[1], "is");
    EXPECT_EQ(tokens[2], "a");
    EXPECT_EQ(tokens[3], "token");
}
