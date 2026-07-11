#ifdef EDITOR_BUILD
#include "AssetCompile/SoundAsset.h"
#include "AssetCompile/Someutils.h"
#include "Framework/Files.h"
#include "Framework/BinaryReadWrite.h"
#include "Framework/SerializerJson.h"
#include "Framework/Util.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

// Defined in TextureEditor.cpp — resolves an import setting's relative src_file against
// the sidecar's own directory. Shared across compilers rather than duplicated (mirrors
// how ModelCompilier.cpp reuses it).
extern std::string turn_gamepath_into_src_path(const std::string& gamepath, const std::string& src_file);

void write_audio_import_settings(AudioImportSettings* ais, const std::string& path) {
	MakePathForGenericObj pathmaker;
	WriteSerializerBackendJson writer("write_ais", pathmaker, *ais, true);

	auto fileptr = FileSys::open_write_game(path);
	if (fileptr) {
		sys_print(Debug, "write_audio_import_settings: writing %s\n", path.c_str());
		std::string out = "!json\n" + writer.get_output().dump(1);
		fileptr->write(out.data(), out.size());
	} else {
		sys_print(Error, "write_audio_import_settings: couldn't open file to write %s\n", path.c_str());
	}
}

static std::unique_ptr<AudioImportSettings> read_audio_import_settings(const std::string& ais_gamepath) {
	auto aisfile = FileSys::open_read_game(ais_gamepath);
	if (!aisfile) {
		sys_print(Warning, "couldn't find audio import settings file %s\n", ais_gamepath.c_str());
		return nullptr;
	}
	std::string to_str(aisfile->size(), ' ');
	aisfile->read(to_str.data(), aisfile->size());
	aisfile->close();

	if (to_str.find("!json") != 0) {
		sys_print(Error, "bad .ais format %s\n", ais_gamepath.c_str());
		return nullptr;
	}
	to_str = to_str.substr(6);
	MakeObjectFromPathGeneric objmaker;
	ReadSerializerBackendJson reader("compile_sound_asset", to_str, objmaker);
	AudioImportSettings* ais = reader.get_root_obj() ? reader.get_root_obj()->cast_to<AudioImportSettings>() : nullptr;
	if (!ais) {
		sys_print(Error, "couldnt parse audio import settings %s\n", ais_gamepath.c_str());
		return nullptr;
	}
	return std::unique_ptr<AudioImportSettings>(ais);
}

// True if the compiled .csnd is missing, stale relative to the .ais/source, or has a bad/old header.
static bool csnd_needs_compile(const std::string& csnd_gamepath, uint64_t ais_timestamp, uint64_t src_timestamp) {
	auto csndfile = FileSys::open_read_game(csnd_gamepath);
	if (!csndfile)
		return true;

	if (csndfile->size() < 4) {
		sys_print(Info, "%s needs compile because header is truncated\n", csnd_gamepath.c_str());
		return true;
	}
	uint32_t magic = 0;
	csndfile->read(&magic, 4);
	if (magic != CSND_MAGIC) {
		sys_print(Info, "%s needs compile because magic mismatch\n", csnd_gamepath.c_str());
		return true;
	}

	uint64_t csnd_timestamp = csndfile->get_timestamp();
	if (csnd_timestamp <= ais_timestamp) {
		sys_print(Debug, "%s needs compile because .ais is newer than .csnd\n", csnd_gamepath.c_str());
		return true;
	}
	if (src_timestamp != 0 && csnd_timestamp <= src_timestamp) {
		sys_print(Debug, "%s needs compile because source audio is newer than .csnd\n", csnd_gamepath.c_str());
		return true;
	}
	return false;
}

static bool run_ffmpeg(const std::string& command_line) {
	sys_print(Info, "executing sound compile: %s\n", command_line.c_str());

	STARTUPINFOA startup = {};
	PROCESS_INFORMATION out = {};
	std::string mutable_cmd = command_line;

	if (!CreateProcessA(nullptr, mutable_cmd.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &out)) {
		sys_print(Error, "couldn't create ffmpeg process\n");
		return false;
	}
	WaitForSingleObject(out.hProcess, INFINITE);
	DWORD exit_code = 0;
	GetExitCodeProcess(out.hProcess, &exit_code);
	CloseHandle(out.hProcess);
	CloseHandle(out.hThread);
	return exit_code == 0;
}

bool compile_sound_asset(const std::string& ais_gamepath) {
	ASSERT(get_extension_no_dot(ais_gamepath) == "ais");

	auto aisfile = FileSys::open_read_game(ais_gamepath);
	if (!aisfile) {
		sys_print(Warning, "couldn't find audio import settings file %s\n", ais_gamepath.c_str());
		return false;
	}
	uint64_t ais_timestamp = aisfile->get_timestamp();
	aisfile->close();

	std::unique_ptr<AudioImportSettings> ais = read_audio_import_settings(ais_gamepath);
	if (!ais)
		return false;

	const std::string src_path = turn_gamepath_into_src_path(ais_gamepath, ais->src_file);
	auto srcfile = FileSys::open_read_game(src_path);
	if (!srcfile) {
		sys_print(Warning, "compile_sound_asset: couldn't open source file %s\n", ais->src_file.c_str());
		return false;
	}
	uint64_t src_timestamp = srcfile->get_timestamp();
	srcfile->close();

	const std::string csnd_gamepath = strip_extension(ais_gamepath) + ".csnd";
	if (!csnd_needs_compile(csnd_gamepath, ais_timestamp, src_timestamp))
		return true;

	const std::string src_full = FileSys::get_full_path_from_game_path(src_path);
	const std::string csnd_full = FileSys::get_full_path_from_game_path(csnd_gamepath);
	const std::string container_ext = (ais->codec == AudioCodec::Vorbis) ? ".ogg" : ".wav";

	// ffmpeg's output MUST live outside the game data directory -- writing it alongside
	// the .csnd (even with an unusual extension like .csnd_tmp.wav) puts a brand-new
	// .wav-looking file in front of the live file-watcher, which auto-imports a bogus
	// .ais sidecar for it before we get a chance to delete it. Use the OS temp dir instead.
	char temp_dir_buf[MAX_PATH] = {};
	GetTempPathA(MAX_PATH, temp_dir_buf);
	const std::string temp_base = fs::path(csnd_gamepath).stem().string();
	const std::string temp_full = std::string(temp_dir_buf) + "csnd_compile_" + temp_base + "_" +
		std::to_string(GetCurrentProcessId()) + container_ext;

	std::string command_line = "ffmpeg.exe -y -i \"" + src_full + "\"";
	if (ais->force_mono)
		command_line += " -ac 1";
	if (ais->sample_rate_override != 0)
		command_line += " -ar " + std::to_string(ais->sample_rate_override);

	switch (ais->codec) {
	case AudioCodec::ADPCM:
		command_line += " -c:a adpcm_ima_wav";
		break;
	case AudioCodec::Vorbis: {
		// vorbis_quality_percent is 0-100 (100=best); ffmpeg's libvorbis -q:a scale is 0-10.
		double q = std::clamp(ais->vorbis_quality_percent, 0, 100) * 10.0 / 100.0;
		command_line += " -c:a libvorbis -q:a " + std::to_string(q);
		break;
	}
	case AudioCodec::PCM:
	default:
		command_line += " -c:a pcm_s16le";
		break;
	}
	command_line += " \"" + temp_full + "\"";

	bool ffmpeg_ok = run_ffmpeg(command_line);
	if (!ffmpeg_ok) {
		sys_print(Error, "ffmpeg failed compiling %s\n", ais_gamepath.c_str());
		std::error_code ec;
		fs::remove(temp_full, ec);
		return false;
	}

	std::error_code ec;
	uint64_t container_size = fs::file_size(temp_full, ec);
	if (ec) {
		sys_print(Error, "compile_sound_asset: ffmpeg produced no output for %s\n", ais_gamepath.c_str());
		return false;
	}

	auto tempfile = FileSys::open_read(temp_full.c_str(), FileSys::FULL_SYSTEM);
	if (!tempfile) {
		sys_print(Error, "compile_sound_asset: couldn't reopen ffmpeg output for %s\n", ais_gamepath.c_str());
		fs::remove(temp_full, ec);
		return false;
	}
	std::vector<uint8_t> container(container_size);
	tempfile->read(container.data(), container.size());
	tempfile->close();

	FileWriter writer((size_t)container_size + 32);
	writer.write_int32(CSND_MAGIC);
	writer.write_int32(SOUND_VERSION);
	writer.write_byte((uint8_t)ais->codec);
	writer.write_byte((uint8_t)ais->load_mode);
	writer.write_float(ais->loop_start_seconds);
	writer.write_float(ais->loop_end_seconds);
	writer.write_int32((uint32_t)container_size);
	writer.write_bytes_ptr(container.data(), container.size());

	auto outfile = FileSys::open_write_game(csnd_gamepath);
	if (!outfile) {
		sys_print(Error, "compile_sound_asset: couldn't write %s\n", csnd_gamepath.c_str());
		fs::remove(temp_full, ec);
		return false;
	}
	outfile->write(writer.get_buffer(), writer.get_size());
	outfile->close();

	fs::remove(temp_full, ec); // best-effort cleanup

	return true;
}

#endif
