#pragma once
#include "Util.h"
#include <memory>
#include <string>
#include <vector>

class IFile
{
public:
	virtual ~IFile() {}
	virtual void close() = 0;
	virtual void read(void* dest, size_t count) = 0;
	virtual size_t size() const = 0;
	virtual bool is_eof() const = 0;
	virtual size_t tell() const = 0;
	virtual void seek(size_t ofs) = 0;
	virtual uint64_t get_timestamp() const { return 0; }
	virtual bool write(const void* dest, size_t count) = 0;
};

using IFilePtr = std::unique_ptr<IFile>;



class FileTreeIterator;
struct FileTreeIter
{
	~FileTreeIter();
	FileTreeIter(const std::string& root, bool dont_descend);
	FileTreeIter();
	FileTreeIter(FileTreeIter&& other);
	const std::string& operator*() const;
	FileTreeIter& operator++();
	bool operator!=(const FileTreeIter& other) const;

	std::unique_ptr<FileTreeIterator> ptr;
};

class FileTree {
public:
	FileTree(const std::string& root) : rootPath(root) {}
	FileTree(const std::string& root, bool dont_descend) : rootPath(root), dont_descend(dont_descend) {}


	FileTreeIter begin() {
		return FileTreeIter(rootPath, dont_descend);
	}

	FileTreeIter end() {
		return FileTreeIter();
	}

private:
	bool dont_descend = false;
	std::string rootPath;
};


class FileSys
{
public:
	enum WhereEnum {
		GAME_DIR = 0,	// searches engine dir and game_dir
		ENGINE_DIR = 1,	// root engine dir
		USER_DIR = 2,	// the save folder location for users
	};

	static void init();

	// open a file for reading, can look in archive or os paths
	static IFilePtr open_read(const char* relative_path, WhereEnum where);
	static IFilePtr open_read_engine(const char* rel) {
		return open_read(rel, ENGINE_DIR);
	}
	static IFilePtr open_read_game(const char* rel) {
		return open_read(rel, GAME_DIR);
	}
	static IFilePtr open_read_game(const std::string& str) {
		return open_read(str.c_str(), GAME_DIR);
	}


	static bool does_file_exist(const char* path, WhereEnum where) {
		return open_read(path, where) != nullptr;
	}

	static FileTree find_files(const char* relative_path) {
		return FileTree(relative_path);
	}
	static FileTree find_game_files() {
		return FileTree(get_path(GAME_DIR));
	}
	static FileTree find_game_files_path(const std::string& path) {
		return FileTree(get_path(GAME_DIR) + ("/" + path));
	}
	static FileTree find_files(const char* relative_path, bool dont_descend) {
		return FileTree(relative_path, dont_descend);
	}

	// opens a file to write, DEFAULT writes to engine dir, GAME_DIR writes to the project dir, etc.
	static IFilePtr open_write(const char* relative_path, WhereEnum where);
	static IFilePtr open_write_game(const char* rel) {
		return open_write(rel, GAME_DIR);
	}
	static IFilePtr open_write_game(const std::string& str) {
		return open_write(str.c_str(), GAME_DIR);
	}

	static const char* get_path(WhereEnum where);
	static const char* get_game_path() {
		return get_path(GAME_DIR);
	}
	static std::string get_full_path_from_game_path(const std::string& game) {
		return get_game_path() + ("/" + game);
	}
	static std::string get_full_path_from_relative(const std::string& relative, WhereEnum where) {
		return get_path(where) + ("/" + relative);
	}
};