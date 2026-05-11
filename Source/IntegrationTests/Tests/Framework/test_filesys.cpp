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

// Test copy_file
static TestTask test_filesys_copy_file(TestContext& t) {
	const char* src_path = "filesys_copy_src_test.bin";
	const char* dst_path = "filesys_copy_dst_test.bin";
	const char test_data[] = "copy_test_data_12345";

	// Write source file
	{
		auto wf = FileSys::open_write_game(src_path);
		t.require(wf != nullptr, "open_write_game succeeded for copy src");
		wf->write(test_data, sizeof(test_data));
	}
	co_await t.wait_ticks(1);

	// Copy the file
	bool copied = FileSys::copy_file(src_path, dst_path, FileSys::GAME_DIR);
	t.check(copied, "copy_file returned true");
	co_await t.wait_ticks(1);

	// Verify both files exist
	t.check(FileSys::does_file_exist(src_path, FileSys::GAME_DIR), "source file still exists after copy");
	t.check(FileSys::does_file_exist(dst_path, FileSys::GAME_DIR), "destination file exists after copy");

	// Verify contents match
	auto rf = FileSys::open_read_game(dst_path);
	t.require(rf != nullptr, "opened copied file for reading");
	t.check(rf->size() == sizeof(test_data), "copied file size matches original");

	char buf[64] = {};
	rf->read(buf, sizeof(test_data));
	t.check(std::memcmp(buf, test_data, sizeof(test_data)) == 0, "copied file contents match original");

	// Cleanup
	FileSys::delete_game_file(src_path);
	FileSys::delete_game_file(dst_path);
}
GAME_TEST("filesys/copy_file", 5.f, test_filesys_copy_file);

// Test move_file
static TestTask test_filesys_move_file(TestContext& t) {
	const char* src_path = "filesys_move_src_test.bin";
	const char* dst_path = "filesys_move_dst_test.bin";
	const char test_data[] = "move_test_data_12345";

	// Write source file
	{
		auto wf = FileSys::open_write_game(src_path);
		t.require(wf != nullptr, "open_write_game succeeded for move src");
		wf->write(test_data, sizeof(test_data));
	}
	co_await t.wait_ticks(1);

	// Move the file
	bool moved = FileSys::move_file(src_path, dst_path, FileSys::GAME_DIR);
	t.check(moved, "move_file returned true");
	co_await t.wait_ticks(1);

	// Verify source is gone and destination exists
	t.check(!FileSys::does_file_exist(src_path, FileSys::GAME_DIR), "source file no longer exists after move");
	t.check(FileSys::does_file_exist(dst_path, FileSys::GAME_DIR), "destination file exists after move");

	// Verify contents are correct
	auto rf = FileSys::open_read_game(dst_path);
	t.require(rf != nullptr, "opened moved file for reading");
	char buf[64] = {};
	rf->read(buf, sizeof(test_data));
	t.check(std::memcmp(buf, test_data, sizeof(test_data)) == 0, "moved file contents are intact");

	// Cleanup
	FileSys::delete_game_file(dst_path);
}
GAME_TEST("filesys/move_file", 5.f, test_filesys_move_file);

// Test does_directory_exist
static TestTask test_filesys_directory_exists(TestContext& t) {
	// Check for known directories that should exist
	bool game_dir_exists = FileSys::does_directory_exist("", FileSys::GAME_DIR);
	t.check(game_dir_exists, "game directory exists");

	// Check for directory that definitely doesn't exist
	bool fake_dir_exists = FileSys::does_directory_exist("nonexistent_dir_xyz123", FileSys::GAME_DIR);
	t.check(!fake_dir_exists, "nonexistent directory does not exist");
	co_return;
}
GAME_TEST("filesys/directory_exists", 5.f, test_filesys_directory_exists);

// Test create_directory
static TestTask test_filesys_create_directory(TestContext& t) {
	const char* test_dir = "filesys_test_new_dir";
	const char* nested_dir = "filesys_test_new_dir/nested";


	// Create the directory
	bool created = FileSys::create_directory(test_dir, FileSys::GAME_DIR);
	t.check(created, "create_directory returned true");
	co_await t.wait_ticks(1);

	// Verify it now exists
	t.check(FileSys::does_directory_exist(test_dir, FileSys::GAME_DIR), "directory exists after creation");

	// Test nested directory creation
	bool nested_created = FileSys::create_directory(nested_dir, FileSys::GAME_DIR);
	t.check(nested_created, "nested create_directory returned true");
	co_await t.wait_ticks(1);

	t.check(FileSys::does_directory_exist(nested_dir, FileSys::GAME_DIR), "nested directory exists after creation");
}
GAME_TEST("filesys/create_directory", 5.f, test_filesys_create_directory);
