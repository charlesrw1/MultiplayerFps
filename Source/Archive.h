#pragma once
#include <unordered_map>
#include <string>
#include "StringUtil.h"
#include "Memory.h"
class Archive_File
{
public:
	char* buffer;
	uint32_t length;
};

struct Archive_Entry
{
	uint32_t string_offset = 0;
	uint32_t data_offset = 0;
	uint32_t data_len = 0;
	uint32_t uncompressed_size = 0;
	uint32_t flags = 0;
};
class Archive
{
public:
	void create(const char* path);

	//Archive_File* open();
	//void close(Archive_File* file);

	std::string path;
	Memory_Arena arena;
	FILE* file_handle;
	std::vector<char> string_table;
	std::vector<Archive_Entry> entries;
	std::unordered_map<uint32_t, int> hash_to_index;
};