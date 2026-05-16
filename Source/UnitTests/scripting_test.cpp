#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
// ClassBase.h must be included before ClassTypeInfo.h (via ScriptManager.h).
// ClassBase.h includes ClassTypeInfo.h at its bottom to complete the circular
// dependency. If ClassTypeInfo.h is processed first, ClassBase.h's include of
// ClassTypeInfo.h is blocked by the include guard, leaving ClassTypeInfo
// incomplete when ClassBase.h's templates are compiled.
#include "Framework/ClassBase.h"
#include "Scripting/ScriptManager.h"
#include "Testheader.h"

// ============================================================
// Part 1: ScriptLoadingUtil::parse_text() — pure unit tests
//
// parse_text() is a pure function: it takes Lua source text and
// returns the class/property structure declared via annotation
// comments. No ClassBase or FileSys involvement.
// ============================================================

TEST(ParseText, EmptyString) {
	EXPECT_TRUE(ScriptLoadingUtil::parse_text("").empty());
}

TEST(ParseText, PlainLuaNoAnnotations) {
	EXPECT_TRUE(ScriptLoadingUtil::parse_text("local x = 5\nfunction foo() end\n").empty());
}

TEST(ParseText, AnnotationWithoutTableBodyNotEmitted) {
	// Class annotation with no matching table body: inClass never becomes true,
	// so the pending class is never pushed to output.
	EXPECT_TRUE(ScriptLoadingUtil::parse_text("---@class Orphan\n").empty());
}

TEST(ParseText, SimpleClassWithEmptyBody) {
	auto r = ScriptLoadingUtil::parse_text("---@class Foo\nFoo = {\n}\n");
	ASSERT_EQ(r.size(), 1u);
	EXPECT_EQ(r[0].name, "Foo");
	EXPECT_TRUE(r[0].inherited.empty());
	EXPECT_TRUE(r[0].props.empty());
}

TEST(ParseText, ClassWithSingleParent) {
	auto r = ScriptLoadingUtil::parse_text("---@class Child : Parent\nChild = {\n}\n");
	ASSERT_EQ(r.size(), 1u);
	EXPECT_EQ(r[0].name, "Child");
	ASSERT_EQ(r[0].inherited.size(), 1u);
	EXPECT_EQ(r[0].inherited[0], "Parent");
}

TEST(ParseText, ClassWithMultipleParents) {
	auto r = ScriptLoadingUtil::parse_text("---@class Child : A, B\nChild = {\n}\n");
	ASSERT_EQ(r.size(), 1u);
	ASSERT_EQ(r[0].inherited.size(), 2u);
	EXPECT_EQ(r[0].inherited[0], "A");
	EXPECT_EQ(r[0].inherited[1], "B");
}

TEST(ParseText, MultipleClassesInFile) {
	const char* src = "---@class Foo\nFoo = {\n}\n"
					  "---@class Bar : Foo\nBar = {\n}\n";
	auto r = ScriptLoadingUtil::parse_text(src);
	ASSERT_EQ(r.size(), 2u);
	EXPECT_EQ(r[0].name, "Foo");
	EXPECT_EQ(r[1].name, "Bar");
	ASSERT_EQ(r[1].inherited.size(), 1u);
	EXPECT_EQ(r[1].inherited[0], "Foo");
}

TEST(ParseText, PropertyWithTypeAnnotation) {
	const char* src = "---@class Foo\n"
					  "Foo = {\n"
					  "---@type number\n"
					  "health = 0,\n"
					  "}\n";
	auto r = ScriptLoadingUtil::parse_text(src);
	ASSERT_EQ(r.size(), 1u);
	ASSERT_EQ(r[0].props.size(), 1u);
	EXPECT_EQ(r[0].props[0].name, "health");
	EXPECT_EQ(r[0].props[0].type_str, "number");
}
/*
TEST(ParseText, PropertyWithoutTypeAnnotation) {
	const char* src = "---@class Foo\n"
					  "Foo = {\n"
					  "speed = 10,\n"
					  "}\n";
	auto r = ScriptLoadingUtil::parse_text(src);
	ASSERT_EQ(r.size(), 1u);
	ASSERT_EQ(r[0].props.size(), 1u);
	EXPECT_EQ(r[0].props[0].name, "speed");
	EXPECT_TRUE(r[0].props[0].type_str.empty());
}
*/

TEST(ParseText, MultipleProperties) {
	const char* src = "---@class Foo\n"
					  "Foo = {\n"
					  "---@type number\n"
					  "health = 100,\n"
					  "---@type string\n"
					  "name = \"\",\n"
					  "}\n";
	auto r = ScriptLoadingUtil::parse_text(src);
	ASSERT_EQ(r.size(), 1u);
	ASSERT_EQ(r[0].props.size(), 2u);
	EXPECT_EQ(r[0].props[0].name, "health");
	EXPECT_EQ(r[0].props[0].type_str, "number");
	EXPECT_EQ(r[0].props[1].name, "name");
	EXPECT_EQ(r[0].props[1].type_str, "string");
}

TEST(ParseText, OneLineClassBody) {
	auto r = ScriptLoadingUtil::parse_text("---@class Foo\nFoo = { }\n");
	ASSERT_EQ(r.size(), 1u);
	EXPECT_EQ(r[0].name, "Foo");
	EXPECT_TRUE(r[0].props.empty());
}

TEST(ParseText, IndentedTableStartNotRecognized) {
	// Table variable must start at column 0 (starts_with check in parser).
	// An indented table declaration doesn't trigger inClass=true.
	auto r = ScriptLoadingUtil::parse_text("---@class Foo\n  Foo = {\n}\n");
	EXPECT_TRUE(r.empty());
}

TEST(ParseText, FirstAnnotationLostWhenSecondArrivesWithoutBody) {
	// When a second ---@class annotation is seen while inClass is false,
	// the first pending class (no body) is silently discarded.
	const char* src = "---@class Lost\n"
					  "---@class Found\n"
					  "Found = {\n}\n";
	auto r = ScriptLoadingUtil::parse_text(src);
	ASSERT_EQ(r.size(), 1u);
	EXPECT_EQ(r[0].name, "Found");
}

/*
TEST(ParseText, FileEndsInsideClassStillEmitted) {
	// No closing brace: inClass=true at EOF means the last class is still emitted.
	const char* src = "---@class Unclosed\n"
					  "Unclosed = {\n"
					  "value = 1,\n";
	auto r = ScriptLoadingUtil::parse_text(src);
	ASSERT_EQ(r.size(), 1u);
	EXPECT_EQ(r[0].name, "Unclosed");
	ASSERT_EQ(r[0].props.size(), 1u);
}
*/
TEST(ParseText, ClassWithCommentBetweenAnnotationAndTable) {
	// Comments between annotation and table should not break parsing
	const char* src = "---@class Foo : Bar\n"
					  "--- This is a comment\n"
					  "Foo = {\n"
					  "}\n";
	auto r = ScriptLoadingUtil::parse_text(src);
	ASSERT_EQ(r.size(), 1u);
	EXPECT_EQ(r[0].name, "Foo");
	ASSERT_EQ(r[0].inherited.size(), 1u);
	EXPECT_EQ(r[0].inherited[0], "Bar");
}

TEST(ParseText, CommentContainingClassKeywordNotMisidentified) {
	// Comments containing "@class" should not be treated as annotations
	const char* src = "---@class Foo : Bar\n"
					  "--- Note: Use @class to define types\n"
					  "Foo = {\n"
					  "---@type number\n"
					  "value = 0,\n"
					  "}\n";
	auto r = ScriptLoadingUtil::parse_text(src);
	ASSERT_EQ(r.size(), 1u);
	EXPECT_EQ(r[0].name, "Foo");
	ASSERT_EQ(r[0].inherited.size(), 1u);
	EXPECT_EQ(r[0].inherited[0], "Bar");
	ASSERT_EQ(r[0].props.size(), 1u);
	EXPECT_EQ(r[0].props[0].name, "value");
}

/*
TEST(ParseText, TypeAnnotationNotCarriedAcrossProperties) {
	// A @type annotation applies only to the immediately following property.
	const char* src = "---@class Foo\n"
					  "Foo = {\n"
					  "---@type number\n"
					  "a = 0,\n"
					  "b = 0,\n"
					  "}\n";
	auto r = ScriptLoadingUtil::parse_text(src);
	ASSERT_EQ(r.size(), 1u);
	ASSERT_EQ(r[0].props.size(), 2u);
	EXPECT_EQ(r[0].props[0].type_str, "number");
	EXPECT_TRUE(r[0].props[1].type_str.empty());
}
*/

TEST(ParseText, FixtureFileTestClassNoInherit) {
	// Parse the fixture file that declares a class with no inheritance.
	auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "TestFiles" / "Scripting" /
				"test_class_no_inherit.lua";
	std::ifstream f(path);
	ASSERT_TRUE(f.is_open()) << "Fixture file missing: " << path;
	std::string src(std::istreambuf_iterator<char>(f), {});

	auto r = ScriptLoadingUtil::parse_text(src);
	ASSERT_EQ(r.size(), 1u);
	EXPECT_EQ(r[0].name, "TestNoInherit");
	EXPECT_TRUE(r[0].inherited.empty());
	ASSERT_EQ(r[0].props.size(), 2u);
	EXPECT_EQ(r[0].props[0].name, "value");
	EXPECT_EQ(r[0].props[0].type_str, "number");
	EXPECT_EQ(r[0].props[1].name, "label");
	EXPECT_EQ(r[0].props[1].type_str, "string");
}
