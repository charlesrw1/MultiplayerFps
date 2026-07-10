#include "RmlUiSystemInterface.h"
#include "Framework/Log.h"
#include <cstdio>
#include <chrono>

double RmlUiSystemInterface::GetElapsedTime() {
	using namespace std::chrono;
	static const auto start = steady_clock::now();
	return duration<double>(steady_clock::now() - start).count();
}

bool RmlUiSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message) {
	switch (type) {
	case Rml::Log::LT_ERROR:
	case Rml::Log::LT_ASSERT:
		sys_print(Error, "[RmlUi] %s\n", message.c_str());
		break;
	case Rml::Log::LT_WARNING:
		sys_print(Warning, "[RmlUi] %s\n", message.c_str());
		break;
	case Rml::Log::LT_DEBUG:
		sys_print(Debug, "[RmlUi] %s\n", message.c_str());
		break;
	default:
		sys_print(Info, "[RmlUi] %s\n", message.c_str());
		break;
	}
	return true;
}

Rml::FileHandle RmlUiFileInterface::Open(const Rml::String& path) {
	IFilePtr file = FileSys::open_read_game(path);
	if (!file)
		return 0;
	Rml::FileHandle handle = next_handle++;
	open_files[handle] = std::move(file);
	return handle;
}

void RmlUiFileInterface::Close(Rml::FileHandle file) {
	auto it = open_files.find(file);
	if (it == open_files.end())
		return;
	it->second->close();
	open_files.erase(it);
}

size_t RmlUiFileInterface::Read(void* buffer, size_t size, Rml::FileHandle file) {
	auto it = open_files.find(file);
	if (it == open_files.end())
		return 0;
	IFile* f = it->second.get();
	const size_t remaining = f->size() - f->tell();
	const size_t to_read = size < remaining ? size : remaining;
	if (to_read == 0)
		return 0;
	f->read(buffer, to_read);
	return to_read;
}

bool RmlUiFileInterface::Seek(Rml::FileHandle file, long offset, int origin) {
	auto it = open_files.find(file);
	if (it == open_files.end())
		return false;
	IFile* f = it->second.get();
	size_t base = 0;
	if (origin == SEEK_SET)
		base = 0;
	else if (origin == SEEK_CUR)
		base = f->tell();
	else if (origin == SEEK_END)
		base = f->size();
	else
		return false;
	f->seek((size_t)((long)base + offset));
	return true;
}

size_t RmlUiFileInterface::Tell(Rml::FileHandle file) {
	auto it = open_files.find(file);
	if (it == open_files.end())
		return 0;
	return it->second->tell();
}
