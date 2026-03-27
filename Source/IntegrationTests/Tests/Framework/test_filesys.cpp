// Source/IntegrationTests/Tests/Framework/test_filesys.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "Framework/Files.h"
#include <cstring>

// Write known bytes to ENGINE_DIR, then read them back and verify content
static TestTask test_filesys_write_read_roundtrip(TestContext& t) {
	const char* test_path = "TestFiles/filesys_rw_test.bin";
	const char test_data[] = "hello_filesys_test_12345";
	const size_t data_len = sizeof(test_data); // includes null terminator

	{
		auto wf = FileSys::open_write(test_path, FileSys::ENGINE_DIR);
		t.require(wf != nullptr, "open_write to ENGINE_DIR/TestFiles succeeded");
		wf->write(test_data, data_len);
	}
	co_await t.wait_ticks(1);

	auto rf = FileSys::open_read(test_path, FileSys::ENGINE_DIR);
	t.require(rf != nullptr, "open_read back after write succeeded");
	t.check(rf->size() == data_len, "file size matches written size");

	char buf[64] = {};
	rf->read(buf, data_len);
	t.check(std::memcmp(buf, test_data, data_len) == 0, "file contents match written data");
}
GAME_TEST("filesys/write_read_roundtrip", 5.f, test_filesys_write_read_roundtrip);

// does_file_exist returns true for a file we know ships with the project
static TestTask test_filesys_exist_true(TestContext& t) {
	bool exists = FileSys::does_file_exist("demo_level_1.tmap", FileSys::GAME_DIR);
	t.check(exists, "demo_level_1.tmap exists in GAME_DIR");
	co_return;
}
GAME_TEST("filesys/exist_true", 5.f, test_filesys_exist_true);

// does_file_exist returns false for a file that definitely does not exist
static TestTask test_filesys_exist_false(TestContext& t) {
	bool exists = FileSys::does_file_exist("nonexistent_xyzzy_abc123.tmap", FileSys::GAME_DIR);
	t.check(!exists, "nonexistent file reports as missing");
	co_return;
}
GAME_TEST("filesys/exist_false", 5.f, test_filesys_exist_false);

// Write a file to GAME_DIR, verify it exists, delete it, verify it is gone
static TestTask test_filesys_delete_game_file(TestContext& t) {
	const char* path = "filesys_delete_test_tmp.bin";
	const char data[] = "delete_me";

	{
		auto wf = FileSys::open_write_game(path);
		t.require(wf != nullptr, "open_write_game succeeded for delete test");
		wf->write(data, sizeof(data));
	}
	co_await t.wait_ticks(1);

	t.require(FileSys::does_file_exist(path, FileSys::GAME_DIR), "temp file exists before deletion");

	bool deleted = FileSys::delete_game_file(path);
	t.check(deleted, "delete_game_file returned true");
	co_await t.wait_ticks(1);

	bool still_exists = FileSys::does_file_exist(path, FileSys::GAME_DIR);
	t.check(!still_exists, "file no longer exists after deletion");
}
GAME_TEST("filesys/delete_game_file", 5.f, test_filesys_delete_game_file);
