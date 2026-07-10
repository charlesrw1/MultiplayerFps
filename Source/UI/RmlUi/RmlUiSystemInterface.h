#pragma once
// Engine-side Rml::SystemInterface + Rml::FileInterface implementations.
// Routes RmlUi's clock/logging through the engine's own utilities, and file
// I/O through FileSys so Data/ui/*.rml resolves the same as other assets
// (archive-aware, GAME_DIR-relative), instead of RmlUi's default fopen path.
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/FileInterface.h>
#include <unordered_map>
#include "Framework/Files.h"

class RmlUiSystemInterface : public Rml::SystemInterface {
public:
	double GetElapsedTime() override;
	bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
};

class RmlUiFileInterface : public Rml::FileInterface {
public:
	Rml::FileHandle Open(const Rml::String& path) override;
	void Close(Rml::FileHandle file) override;
	size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
	bool Seek(Rml::FileHandle file, long offset, int origin) override;
	size_t Tell(Rml::FileHandle file) override;

private:
	Rml::FileHandle next_handle = 1;
	std::unordered_map<Rml::FileHandle, IFilePtr> open_files;
};
