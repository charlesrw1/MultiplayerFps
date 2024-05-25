#include "Framework/Util.h"
#include <unordered_map>
#include <string>
#include "Framework/StringUtil.h"
#include <Windows.h>
#include "Framework/Files.h"
struct File_Internal
{
	File_Buffer external_buffer;
	std::vector<char> data;
	bool is_archive_file = false;
	File_Internal* next = nullptr;
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
	bool open(const char* file, File_Internal* outfile);

	FILE* file_handle;
	std::vector<char> string_table;
	std::vector<Archive_Entry> entries;
	std::unordered_map<uint32_t, int> hash_to_index;

	Archive* next = nullptr;
};


void Archive::create(const char* archive_path)
{
	file_handle = std::fopen(archive_path, "rb");
	if (!file_handle) return;

	char magic[4];

	std::fread(magic, 1, 4, file_handle);
	if (magic[0] != 'A' || magic[1] != 'B' || magic[2] != 'C' || magic[3] != 'D') {
		printf("Bad magic\n");
		std::fclose(file_handle);
		return;
	}

	uint32_t version;
	std::fread(&version, 1, 4, file_handle);
	printf("Archive version %d\n", version);

	uint32_t num_entries;
	std::fread(&num_entries, 1, 4, file_handle);
	uint32_t data_offset;
	std::fread(&data_offset, 1, 4, file_handle);

	uint32_t string_offset = 16 + num_entries * 20;
	uint32_t string_table_len = data_offset - string_offset;
	string_table.resize(string_table_len);
	std::fseek(file_handle,string_offset,SEEK_SET);
	std::fread(string_table.data(), 1, string_table.size(), file_handle);

	std::fseek(file_handle, 16, SEEK_SET);
	entries.resize(num_entries);
	std::fread(entries.data(), 1, num_entries * sizeof(Archive_Entry), file_handle);

	for (unsigned int i = 0; i < num_entries; i++) {
		Archive_Entry& e = entries[i];
		StringUtils::StringHash hash(&string_table.at(e.string_offset));
		if (hash_to_index.find(hash.computedHash) != hash_to_index.end()) {
			printf("Collision! %s\n", &string_table.at(e.string_offset));
		}
		hash_to_index[hash.computedHash] = i;
	}
}

bool Archive::open(const char* file, File_Internal* outfile)
{
	const char* found = strstr(file, "./Data/");
	if (found != nullptr) file += 7;

	StringUtils::StringHash hash(file);
	auto find = hash_to_index.find(hash.computedHash);
	if (find == hash_to_index.end()) return false;

	Archive_Entry& entry = entries.at(find->second);

	if (outfile->data.size() < entry.data_len)
		outfile->data.resize(entry.data_len);
	outfile->external_buffer.buffer = outfile->data.data();
	outfile->external_buffer.length = entry.data_len;

	std::fseek(file_handle, entry.data_offset, SEEK_SET);
	std::fread(outfile->data.data(), 1, entry.data_len, file_handle);

	return true;
}

#include <fstream>


File_Internal* file_free_list = nullptr;
Archive* archives = nullptr;

File_Buffer* Files::open(const char* file, int flags)
{
	File_Internal* f = nullptr;
	if (file_free_list) {
		f = file_free_list;
		file_free_list = file_free_list->next;
		f->next = nullptr;
	}
	else {
		f = new File_Internal;
	}

	if (flags & LOOK_IN_ARCHIVE) {
		Archive* a = archives;
		while (a)
		{
			bool good = a->open(file, f);
			if (good) {
				return &f->external_buffer;
			}
			a = a->next;
		}
	}

	// couldn't find file in archive, check data directory

	std::string name(file);
	//name = "./Data/";
	//name = file;
	bool binary = true;// !(flags & TEXT);

	std::ifstream infile(name.c_str(), (binary) ? std::ios::binary : 1);
	if (!infile) {
		return nullptr;
	}

	infile.seekg(0, std::ios_base::end);
	size_t length = infile.tellg();

	// avoid memseting it all to 0 if you dont need to
	if(f->data.size()<length)
		f->data.resize(length);

	infile.seekg(0);
	infile.read((char*)f->data.data(), length);
	infile.close();

	f->external_buffer.buffer = f->data.data();
	f->external_buffer.length = length;
	
	return &f->external_buffer;
}

void Files::close(File_Buffer*& file)
{
	File_Internal* f = (File_Internal*)file;
	file = nullptr;

	f->next = file_free_list;
	file_free_list = f;
}

void Files::init()
{
	Archive* a = new Archive;
	a->create("archive.dat");
	archives = a;
}

bool Files::does_file_exist(const char* path)
{
	if (INVALID_FILE_ATTRIBUTES == GetFileAttributesA(path) && GetLastError() == ERROR_FILE_NOT_FOUND) {
		return false;
	}
	return true;
}
struct FileListIteratorImp : public FileListIterator {

	FileListIteratorImp() {

	}
	~FileListIteratorImp() override {

	}

	// Inherited via FileListIterator
	virtual bool next() override
	{
		return false;
	}
	virtual std::string&& get_path() override
	{
		return std::string && ();
	}
	virtual uint64_t get_timestamp() override
	{
		return uint64_t();
	}

	bool descend_subdirectories = false;
	HANDLE hFind = nullptr;
	WIN32_FIND_DATAA findData;
	std::string path;
};
bool Files::iterate_files_in_dir(const char* dir, char* buffer, int buffer_len)
{
	static bool in_middle_of_search = false;
	static HANDLE hFind = nullptr;
	static WIN32_FIND_DATAA findData;
	
	if (!in_middle_of_search) {
		findData = {};
		hFind = FindFirstFileA(dir, &findData);
		if (hFind == INVALID_HANDLE_VALUE)
			return false;
	}
	in_middle_of_search = true;

	while (FindNextFileA(hFind, &findData) != 0) {
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY || findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
			continue;
		int len = strlen(findData.cFileName);
		if (len + 1 <= buffer_len) {
			memcpy(buffer, findData.cFileName, len);
			buffer[len] = 0;
			return true;
		}
	}
	in_middle_of_search = false;
	FindClose(hFind);
	return false;
}

bool file_getline(const File_Buffer* file, Buffer* str_buffer, int* index, char delimiter)
{
	int start = *index;
	int i = start;
	int length = file->length;
	for (; i < length; i++) {
		if (file->buffer[i] == delimiter) {
			int end = i;
			if (end > 0 && file->buffer[end - 1] == '\r') end = end - 1;
			int size = end - start;
			if (size < 0) size = 0;
			if (size > str_buffer->length - 1) {
				return false;
			}
			for (int j = start; j < end; j++) {
				str_buffer->buffer[j - start] = file->buffer[j];
			}
			str_buffer->buffer[size] = 0;
			*index = i+1;
			return true;
		}
	}
	if (i == length) {
		int end = i;
		if (end > 0 && file->buffer[end - 1] == '\r') end = end - 1;
		int size = end - start;
		if (size < 0) size = 0;
		if (size > str_buffer->length - 1) {
			return false;
		}
		for (int j = start; j < end; j++) {
			str_buffer->buffer[j - start] = file->buffer[j];
		}
		str_buffer->buffer[size] = 0;
		*index = length + 1;
		return true;
	}

	return false;
}