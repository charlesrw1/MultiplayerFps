#include <gtest/gtest.h>
#include "Framework/StringName.h"

TEST(StringNameTest, EqualStringsHashEqual) {
	StringName a("foo");
	StringName b("foo");
	EXPECT_EQ(a, b);
	EXPECT_EQ(a.get_hash(), b.get_hash());
}

TEST(StringNameTest, DifferentStringsHashDifferent) {
	StringName a("foo");
	StringName b("bar");
	EXPECT_NE(a, b);
}

TEST(StringNameTest, DefaultAndEmptyAreNull) {
	StringName def;
	StringName empty("");
	EXPECT_TRUE(def.is_null());
	EXPECT_TRUE(empty.is_null());
	EXPECT_EQ(def, empty);
}

TEST(StringNameTest, GetCStrReturnsOriginalName) {
	StringName a("some_unique_test_name_12345");
	EXPECT_STREQ(a.get_c_str(), "some_unique_test_name_12345");
}

// intern() must be bit-for-bit compatible with the runtime constructor: both feed the same
// fnv1a_64 with the same (ptr,len) semantics, since Lua-authored and C++-authored names for
// the same identifier need to collide in get_stringname_from_lua/push_stringname_to_lua.
TEST(StringNameTest, InternMatchesConstructorHash) {
	const char* s = "matching_name";
	StringName fromCtor(s);
	StringName fromIntern = StringName::intern(s, StringUtils_Hash::const_strlen(s));
	EXPECT_EQ(fromCtor, fromIntern);
	EXPECT_STREQ(fromIntern.get_c_str(), s);
}

TEST(StringNameTest, InternEmptyLenIsNull) {
	StringName n = StringName::intern("ignored", 0);
	EXPECT_TRUE(n.is_null());
}

// The NAME() compile-time macro hashes at compile time; it must agree with the runtime path
// so a C++-side NAME("x") and a Lua-side LuaSystem.name("x") refer to the same StringName.
TEST(StringNameTest, CompileTimeMacroMatchesRuntime) {
	StringName compileTime = NAME("compile_time_name");
	StringName runtime("compile_time_name");
	EXPECT_EQ(compileTime, runtime);
}

TEST(StringNameTest, HashFromRawValueRoundTrips) {
	StringName original("round_trip_me");
	name_hash_t h = original.get_hash();
	StringName rebuilt(h);
	EXPECT_EQ(original, rebuilt);
	// Debug name still resolves since the original construction registered it.
	EXPECT_STREQ(rebuilt.get_c_str(), "round_trip_me");
}
