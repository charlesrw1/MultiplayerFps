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
};

using IFilePtr = std::unique_ptr<IFile>;



class FileTreeIterator;
struct FileTreeIter
{
    ~FileTreeIter();
    FileTreeIter(const std::string& root);
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

    FileTreeIter begin() {
        return FileTreeIter(rootPath);
    }

    FileTreeIter end() {
        return FileTreeIter(); 
    }

private:
    std::string rootPath;
};


class FileSys
{
public:
	enum Flags {
		DONT_SEARCH_ARCHIVES = 1,
		TEXT = 2,
	};
	static void init();

	// open a file for reading, can look in archive or os paths
	static IFilePtr open_read(const char* path, int flags = 0 /* Flags enum */);

	static IFilePtr open_read_os(const char* path) {
		return open_read(path, DONT_SEARCH_ARCHIVES);
	}
	static bool does_os_file_exist(const char* path) {
		return open_read(path, DONT_SEARCH_ARCHIVES) != nullptr;
	}

	static FileTree find_files(const char* relative_path) {
		return FileTree(relative_path);
	}
};