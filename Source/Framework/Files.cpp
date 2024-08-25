

#include "Framework/Util.h"
#include <unordered_map>
#include <string>
#include "Framework/StringUtil.h"
#include <Windows.h>
#include "Framework/Files.h"
#include "Framework/BinaryReadWrite.h"
#include <fstream>
#include <direct.h>
#include "Config.h"

static ConfigVar g_project_base("g_project_base", "gamedat", CVAR_DEV, "what folder to search for assets in");
static ConfigVar g_user_save_dir("g_user_save_dir", "user", CVAR_DEV, "what folder to save user config/settings to");

static ConfigVar file_print_all_openfile_fails("file_print_all_openfile_fails", "0", CVAR_DEV | CVAR_BOOL, "prints an error log for all CreateFile errors");

class OSFile : public IFile
{
public:
	virtual ~OSFile() override {
		close();
	}

	void init(const char* path) {
		winhandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (winhandle != INVALID_HANDLE_VALUE) {
			len = GetFileSize(winhandle, nullptr);
		}
		else if (file_print_all_openfile_fails.get_bool())
			sys_print("!!! OSFile failed to open read: %s\n", path);

	}
	void init_write(const char* path) {
		winhandle = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (winhandle == INVALID_HANDLE_VALUE && file_print_all_openfile_fails.get_bool())
			sys_print("!!! OSFile failed to open write: %s\n", path);
	}

	// Inherited via IFile
	virtual void close() override
	{
		if (winhandle != INVALID_HANDLE_VALUE)
			CloseHandle(winhandle);
		winhandle = INVALID_HANDLE_VALUE;
	}
	virtual void read(void* dest, size_t count) override
	{
		DWORD bytesread{};
		bool good = ReadFile(winhandle, dest, count, &bytesread, nullptr);

		if (bytesread == 0 && good)
			eof_triggered = true;

	}
	virtual size_t size() const override
	{
		return len;
	}
	virtual bool is_eof() const override
	{
		return eof_triggered;
	}
	virtual size_t tell() const override
	{
		auto where_ = SetFilePointer(winhandle, 0, nullptr, FILE_CURRENT);
		return where_;
	}
	virtual void seek(size_t ofs) override
	{
		SetFilePointer(winhandle, ofs, nullptr, FILE_BEGIN);
	}
	virtual uint64_t get_timestamp() const override {
		FILETIME ft;
		GetFileTime(winhandle, nullptr, nullptr, &ft);
		return (uint64_t)ft.dwLowDateTime | ((uint64_t)ft.dwHighDateTime << 32);
	}
	bool write(const void* data, size_t size) override {
		bool good = WriteFile(winhandle, data, size, nullptr, nullptr);
		return good;
	}

	bool handle_is_valid() const { return winhandle != INVALID_HANDLE_VALUE; }
	bool eof_triggered = false;
	size_t len = 0;
	HANDLE winhandle = INVALID_HANDLE_VALUE;
};


// a basic archive file
struct OneArchiveFile
{
	uint32_t data_offset = 0;
	uint32_t data_len = 0;
	uint32_t uncompressed_size = 0;
};
class ArchiveFile
{
public:
	~ArchiveFile() {
	}

	bool open(const char* file);

	IFilePtr file = nullptr;
	std::unordered_map<std::string, OneArchiveFile> hash_to_index;
};

class PackagedFile : public IFile
{

public:

	bool handle_is_valid() const { return self; }

	void seek(size_t ofs) override
	{
		parentFile->file->seek(self->data_offset + ofs);
	}
	size_t tell() const override
	{
		return parentFile->file->tell() - self->data_offset;
	}

	bool write(const void* data, size_t size) override {
		assert(0);
		Fatalf("cant write to packaged file\n");
		return false;
	}

	bool is_eof() const override
	{
		return eof_triggered;
	}

	size_t size() const override {
		return self->data_len;
	}
	void close() override
	{

	}
	void read(void* dest, size_t count) override
	{
		parentFile->file->read(dest, count);
	}

	bool eof_triggered = false;
	size_t ptr = 0;
	OneArchiveFile* self = nullptr;
	ArchiveFile* parentFile = nullptr;
};

bool ArchiveFile::open(const char* archive_path)
{
	///file = FileSys::open_read(archive_path);

	if (!file) {
		sys_print("!!! couldn't open archive file %s\n", archive_path);
		return false;
	}

	char magic[4];
	file->read(magic, 4);

	if (magic[0] != 'A' || magic[1] != 'B' || magic[2] != 'C' || magic[3] != 'D') {
		sys_print("!!! bad archive file %s\n", archive_path);
		file->close();
		return false;
	}

	uint32_t version;
	file->read(&version, 4);

	if (version != 2) {
		sys_print("!!! archive file out of data %s, has version %d not 2\n", archive_path, version);
		file->close();
		return false;
	}

	uint32_t num_entries;
	file->read(&num_entries, 4);
	uint32_t data_offset;
	file->read(&data_offset, 1);

	uint32_t string_offset = 16 + num_entries * 20;
	uint32_t string_table_len = data_offset - string_offset;


	std::vector<char> string_table;
	string_table.resize(string_table_len);

	file->seek(string_offset);
	file->read(string_table.data(), string_table_len);

	file->seek(16);

}

IFilePtr open_read_dir(const std::string& root, const std::string& relative)
{
	auto fullpath = root + "/" + relative;
	OSFile* file = new OSFile;
	file->init(fullpath.c_str());
	if (file->handle_is_valid())
		return IFilePtr(file);
	delete file;
	return nullptr;
}
IFilePtr open_write_dir(const std::string& root, const std::string& relative)
{
	auto fullpath = root + "/" + relative;
	OSFile* file = new OSFile;
	file->init_write(fullpath.c_str());
	if (file->handle_is_valid())
		return IFilePtr(file);
	delete file;
	return nullptr;
}

IFilePtr FileSys::open_read(const char* p, WhereEnum flags)
{

	if (flags == FileSys::USER_DIR) {
		return open_read_dir(g_user_save_dir.get_string(), p);
	}
	else if (flags == FileSys::GAME_DIR) {
		return open_read_dir(g_project_base.get_string(), p);
	}
	else if (flags == FileSys::ENGINE_DIR) {
		return open_read_dir(".", p);
	}
	assert(0);

	return nullptr;
}
IFilePtr FileSys::open_write(const char* relative_path, WhereEnum where)
{
	if (where == FileSys::USER_DIR) {
		return open_write_dir(g_user_save_dir.get_string(), relative_path);
	}
	else if (where == FileSys::GAME_DIR) {
		return open_write_dir(g_project_base.get_string(), relative_path);
	}
	else if (where == FileSys::ENGINE_DIR) {
		return open_write_dir(".", relative_path);
	}
	assert(0);

	return nullptr;
}
const char* FileSys::get_path(WhereEnum where)
{
	if (where == USER_DIR)
		return g_user_save_dir.get_string();
	else if (where == GAME_DIR)
		return g_project_base.get_string();
	else if (where == ENGINE_DIR)
		return ".";
}

std::string FileSys::get_game_path_from_full_path(const std::string& fullpath) {
	std::string gamedir = get_path(GAME_DIR);
	gamedir += "/";
	int i = 0;
	for (; i < gamedir.size() && i < fullpath.size(); i++)
		if (gamedir[i] != fullpath[i])
			break;
	if (i != gamedir.size()) {
		sys_print("??? get_game_path_from_full_path not a game path\n");
		return fullpath;
	}
	return fullpath.substr(gamedir.size());
}

void FileSys::init()
{
	sys_print("------ FileSys init ------\n");

	// updates working directory to ./CsRemake/*
	char own_path[256];
	HMODULE hModule = GetModuleHandleA(NULL);
	if (hModule != NULL)
	{
		GetModuleFileNameA(hModule, own_path, (sizeof(own_path)));
		std::string path = own_path;
		auto find = path.rfind('\\');
		if (find == std::string::npos)
			Fatalf("!!! bad getmodulefilename\n");
		path = path.substr(0, find);
		path += "\\..\\..\\";
		int ret = _chdir(path.c_str());
		if (ret != 0)
			Fatalf("!!! failed to change working directory %d %s\n", ret, path.c_str());
	}
	else {
		Fatalf("!!! GetModuleHandle returned null\n");
	}

}

class FileTreeIterator {
public:
	struct FileEntry {
		std::string path;
		WIN32_FIND_DATAA findData;
	};

	FileTreeIterator(const std::string& root, bool dont_descend) : dont_descend(dont_descend) {
		pushDirectory(root);
		++(*this);  // Move to the first valid file
	}

	FileTreeIterator() {
	}

	~FileTreeIterator() {
		closeCurrentHandle();
	}

	const FileEntry& operator*() const {
		return currentEntry;
	}

	FileTreeIterator& operator++() {
		while (!directories.empty()) {
			if (FindNextFileA(directories.back().handle, &currentEntry.findData)) {
				std::string filename = currentEntry.findData.cFileName;
				if (filename != "." && filename != "..") {
					currentEntry.path = directories.back().path + "/" + filename;
					if (!dont_descend && (currentEntry.findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
						pushDirectory(currentEntry.path);
						continue;
					}
					return *this;
				}
			}
			else {
				closeCurrentHandle();
				directories.pop_back();
			}
		}

		currentEntry = FileEntry();  // Reset current entry to end state
		return *this;
	}

	bool operator!=(const FileTreeIterator& other) const {
		return !directories.empty() || !other.directories.empty();
	}

private:
	struct DirectoryEntry {
		HANDLE handle;
		std::string path;
	};

	std::vector<DirectoryEntry> directories;
	FileEntry currentEntry;

	bool dont_descend = false;

	void pushDirectory(const std::string& directory) {
		std::string searchPath = directory + "/*";
		HANDLE handle = FindFirstFileA(searchPath.c_str(), &currentEntry.findData);
		if (handle != INVALID_HANDLE_VALUE) {
			directories.push_back({ handle, directory });
		}
	}

	void closeCurrentHandle() {
		if (!directories.empty() && directories.back().handle != INVALID_HANDLE_VALUE) {
			FindClose(directories.back().handle);
		}
	}
};


FileTreeIter::~FileTreeIter()
{

}

FileTreeIter::FileTreeIter()
{
	ptr = std::make_unique<FileTreeIterator>();
}

FileTreeIter::FileTreeIter(FileTreeIter&& other) {
	ptr = std::move(other.ptr);
}

FileTreeIter::FileTreeIter(const std::string& root, bool dont_descend) {
	ptr = std::make_unique<FileTreeIterator>(root, dont_descend);
}


const std::string& FileTreeIter::operator*() const {
	return ptr->operator*().path;
}

bool FileTreeIter::operator!=(const FileTreeIter& other) const {
	return *ptr != *other.ptr;
}

FileTreeIter& FileTreeIter::operator++() {
	++(*ptr);
	return *this;
}


bool FileWriter::write_out(const std::string& path)
{
	std::ofstream outfile(path);
	if (!outfile)
		return false;
	outfile.write((char*)buffer.data(), buffer.size());
	outfile.close();

	return true;
}
