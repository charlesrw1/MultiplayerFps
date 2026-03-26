#include <gtest/gtest.h>
#include "Framework/MapUtil.h"
#include "Framework/StringUtil.h"
#include <unordered_map>
#include <string>

// ---- MapUtil ----

TEST(MapUtilTest, ContainsFound) {
	std::unordered_map<std::string, int> m;
	m["key"] = 1;
	EXPECT_TRUE(MapUtil::contains(m, std::string("key")));
}

TEST(MapUtilTest, ContainsNotFound) {
	std::unordered_map<std::string, int> m;
	EXPECT_FALSE(MapUtil::contains(m, std::string("missing")));
}

TEST(MapUtilTest, GetOrReturnsValueWhenFound) {
	std::unordered_map<std::string, int> m;
	m["x"] = 99;
	EXPECT_EQ(MapUtil::get_or(m, std::string("x"), 0), 99);
}

TEST(MapUtilTest, GetOrReturnsFallbackWhenMissing) {
	std::unordered_map<std::string, int> m;
	EXPECT_EQ(MapUtil::get_or(m, std::string("missing"), 42), 42);
}

TEST(MapUtilTest, GetOptReturnsPointerWhenFound) {
	std::unordered_map<std::string, int> m;
	m["a"] = 7;
	const int* p = MapUtil::get_opt(m, std::string("a"));
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(*p, 7);
}

TEST(MapUtilTest, GetOptReturnsNullWhenMissing) {
	std::unordered_map<std::string, int> m;
	const int* p = MapUtil::get_opt(m, std::string("nope"));
	EXPECT_EQ(p, nullptr);
}

TEST(MapUtilTest, GetOptMutableAllowsWrite) {
	std::unordered_map<std::string, int> m;
	m["b"] = 3;
	int* p = MapUtil::get_opt(m, std::string("b"));
	ASSERT_NE(p, nullptr);
	*p = 99;
	EXPECT_EQ(m["b"], 99);
}

// ---- Stack_String ----

TEST(StackStringTest, BasicConstruct) {
	Stack_String<64> s("hello");
	EXPECT_EQ(s.size(), 5);
	EXPECT_STREQ(s.c_str(), "hello");
}

TEST(StackStringTest, DefaultIsEmpty) {
	Stack_String<64> s;
	EXPECT_EQ(s.size(), 0);
	EXPECT_STREQ(s.c_str(), "");
}

TEST(StackStringTest, ConstructWithLen) {
	Stack_String<64> s("hello world", 5);
	EXPECT_EQ(s.size(), 5);
	EXPECT_STREQ(s.c_str(), "hello");
}

// ---- StringView ----

TEST(StringViewTest, DefaultIsEmpty) {
	StringView sv;
	EXPECT_TRUE(sv.is_empty());
	EXPECT_EQ(sv.str_len, 0);
}

TEST(StringViewTest, ConstructFromCStr) {
	StringView sv("test");
	EXPECT_EQ(sv.str_len, 4);
	EXPECT_FALSE(sv.is_empty());
}

TEST(StringViewTest, CmpMatchesExact) {
	StringView sv("hello");
	EXPECT_TRUE(sv.cmp("hello"));
	EXPECT_FALSE(sv.cmp("hell"));
	EXPECT_FALSE(sv.cmp("helloo"));
	EXPECT_FALSE(sv.cmp("world"));
}

TEST(StringViewTest, EqualityOperator) {
	StringView a("abc");
	StringView b("abc");
	StringView c("xyz");
	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a == c);
}

TEST(StringViewTest, ConstructWithExplicitLen) {
	const char* str = "hello world";
	StringView sv(str, 5);
	EXPECT_EQ(sv.str_len, 5);
	EXPECT_TRUE(sv.cmp("hello"));
}

// ---- FNV hash ----

TEST(StringHashTest, SameStringSameHash32) {
	using namespace StringUtils_Hash;
	constexpr uint32_t h1 = fnv1a_32("test", 4);
	constexpr uint32_t h2 = fnv1a_32("test", 4);
	EXPECT_EQ(h1, h2);
}

TEST(StringHashTest, DifferentStringsDifferentHash32) {
	using namespace StringUtils_Hash;
	constexpr uint32_t h1 = fnv1a_32("foo", 3);
	constexpr uint32_t h2 = fnv1a_32("bar", 3);
	EXPECT_NE(h1, h2);
}

TEST(StringHashTest, SameStringSameHash64) {
	using namespace StringUtils_Hash;
	constexpr uint64_t h1 = fnv1a_64("hello", 5);
	constexpr uint64_t h2 = fnv1a_64("hello", 5);
	EXPECT_EQ(h1, h2);
}

TEST(StringHashTest, DifferentStringsDifferentHash64) {
	using namespace StringUtils_Hash;
	constexpr uint64_t h1 = fnv1a_64("abc", 3);
	constexpr uint64_t h2 = fnv1a_64("def", 3);
	EXPECT_NE(h1, h2);
}

TEST(StringHashTest, StringHashStructWrapsCorrectly) {
	using namespace StringUtils_Hash;
	StringHash h1("mystring");
	StringHash h2("mystring");
	EXPECT_EQ(h1.computedHash, h2.computedHash);

	StringHash h3("other");
	EXPECT_NE(h1.computedHash, h3.computedHash);
}
