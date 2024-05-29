#include "Framework/Util.h"
#include <unordered_map>
#include <string>
#include "Framework/StringUtil.h"
#include <Windows.h>
#include "Framework/Files.h"
#include "Framework/BinaryReadWrite.h"
#include <fstream>

class OSFile : public IFile
{
public:
	virtual ~OSFile() override {
		close();
	}

	void init(const char* path) {
		winhandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (winhandle!= INVALID_HANDLE_VALUE) {
			len =GetFileSize(winhandle, nullptr);
		}
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
	bool handle_is_valid() const { return winhandle != INVALID_HANDLE_VALUE; }
	bool eof_triggered = false;
	size_t len = 0;
	HANDLE winhandle = INVALID_HANDLE_VALUE;
};


struct ArchiveFiles
{
	uint32_t data_offset = 0;
	uint32_t data_len = 0;
	uint32_t uncompressed_size = 0;
};

class Archive
{
public:
	~Archive() {
	}

	bool open(const char* file);

	IFilePtr file = nullptr;
	std::unordered_map<std::string, ArchiveFiles> hash_to_index;
};


bool Archive::open(const char* archive_path)
{
	file = FileSys::open_read_os(archive_path);
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

IFilePtr FileSys::open_read(const char* p, int flags)
{
	OSFile* file = new OSFile;
	file->init(p);

	if (!file->handle_is_valid()) {
		delete file;
		return nullptr;
	}

	return IFilePtr(file);
}



void FileSys::init()
{
	
}

class FileTreeIterator {
public:
	struct FileEntry {
		std::string path;
		WIN32_FIND_DATAA findData;
	};

	FileTreeIterator(const std::string& root) {
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
					if (currentEntry.findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
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

FileTreeIter::FileTreeIter(const std::string& root) {
	ptr = std::make_unique<FileTreeIterator>(root);
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