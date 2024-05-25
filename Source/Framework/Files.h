#pragma once
#include "Util.h"
#include <memory>

typedef Buffer File_Buffer;

// useful helper for txt data files
bool file_getline(const File_Buffer* file, Buffer* str_buffer, int* index, char delimiter = '\n');

class FileReader;
using FileReaderPtr = std::unique_ptr<FileReader>;
class FileWriter;

struct FileListIterator {
	virtual ~FileListIterator() = 0;
	virtual bool next() = 0;
	virtual std::string&& get_path() = 0;
	virtual uint64_t get_timestamp() = 0;
};
using FileListIteratorPtr = std::unique_ptr<FileListIterator>;


class Files
{
public:
	enum {
		LOOK_IN_ARCHIVE = 1,
		TEXT = 2,
	};

	static FileReaderPtr open_read(const char* path);
	static FileListIteratorPtr iterate_dir(const char* path, bool descend_subdirectories);
	static bool does_file_exist(const char* path) {
		return open_read(path) != nullptr;
	}
	static void open_archive(const char* path);


	static File_Buffer* open(const char* path, int flags = LOOK_IN_ARCHIVE);
	static void close(File_Buffer*& file);
	static void init();
	static bool iterate_files_in_dir(const char* path, char* buffer, int buffer_size);
	static bool does_file_exist(const char* path);

	static bool
};