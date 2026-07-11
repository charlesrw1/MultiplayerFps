#ifdef EDITOR_BUILD
#include "Assets/AssetReferenceQuery.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetRegistryLocal.h"
#include "Framework/Files.h"
#include "Framework/Log.h"
#include "Framework/Util.h"
#include "AssetCompile/Someutils.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <sstream>

static std::string read_pipe_to_string(HANDLE pipe) {
	std::string out;
	char buf[4096];
	DWORD bytesRead = 0;
	while (ReadFile(pipe, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0)
		out.append(buf, bytesRead);
	return out;
}

// Runs "rg <args>" (relies on rg being on PATH) and captures stdout. stderr is
// captured separately and only logged - it must never be merged into stdout,
// since a CLI error there (unsupported flag, bad regex, ...) would otherwise
// get silently parsed as if it were real match data.
// Non-fatal if rg finds nothing (exit code 1) - callers just get an empty string.
static std::string run_ripgrep_captured(const std::string& args) {
	SECURITY_ATTRIBUTES sa = {};
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	HANDLE outRead = nullptr, outWrite = nullptr;
	HANDLE errRead = nullptr, errWrite = nullptr;
	if (!CreatePipe(&outRead, &outWrite, &sa, 0) || !CreatePipe(&errRead, &errWrite, &sa, 0)) {
		sys_print(Error, "AssetReferenceQuery: CreatePipe failed\n");
		return {};
	}
	SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOA si = {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = outWrite;
	si.hStdError = errWrite;
	si.hStdInput = nullptr;

	PROCESS_INFORMATION pi = {};
	std::string commandLine = "rg " + args;

	sys_print(Debug, "AssetReferenceQuery: running: %s\n", commandLine.c_str());
	double t0 = GetTime();

	const bool created = CreateProcessA(nullptr, (char*)commandLine.c_str(), nullptr, nullptr, TRUE,
		CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
	CloseHandle(outWrite);
	CloseHandle(errWrite);

	if (!created) {
		sys_print(Error, "AssetReferenceQuery: couldn't create rg process (is rg on PATH?)\n");
		CloseHandle(outRead);
		CloseHandle(errRead);
		return {};
	}

	std::string output = read_pipe_to_string(outRead);
	std::string errors = read_pipe_to_string(errRead);

	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	CloseHandle(outRead);
	CloseHandle(errRead);

	if (!errors.empty())
		sys_print(Warning, "AssetReferenceQuery: rg stderr: %s\n", errors.c_str());

	sys_print(Debug, "AssetReferenceQuery: took %.1f ms, %zu bytes output\n", (GetTime() - t0) * 1000.0, output.size());
	return output;
}

static std::vector<std::string> split_lines(const std::string& s) {
	std::vector<std::string> out;
	std::stringstream ss(s);
	std::string line;
	while (std::getline(ss, line, '\n')) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (!line.empty())
			out.push_back(std::move(line));
	}
	return out;
}

static void normalize_slashes(std::string& s) {
	std::replace(s.begin(), s.end(), '\\', '/');
}

// Union of every registered asset type's extensions, deduped, order-stable.
static const std::vector<std::string>& get_all_known_extensions() {
	static std::vector<std::string> exts;
	if (exts.empty()) {
		std::unordered_set<std::string> seen;
		for (auto& type : AssetRegistrySystem::get().get_types())
			for (auto& ext : type->extensions)
				if (seen.insert(ext).second)
					exts.push_back(ext);
	}
	return exts;
}

// Every asset format is plain text except .cmdl (which embeds material path
// strings in an otherwise binary container). Leaf binary formats (compiled
// textures/audio/source meshes) never embed outward references, so they're
// excluded as backward-search targets - scanning them with -a would force rg
// to read their full (often multi-MB) binary content instead of skipping via
// its normal binary-detection fast path.
static bool is_leaf_binary_ext(const std::string& ext) {
	static const std::unordered_set<std::string> leaf = {"dds", "wav", "png", "jpg", "jpeg", "hdr", "glb", "blend", "ssbar"};
	return leaf.count(ext) != 0;
}

// Every leaf binary format is excluded from the directory walk via --glob so
// rg's fast binary-skip heuristic never has to fire and -a never forces it to
// read multi-MB texture/audio content - .cmdl is deliberately left in since it
// is the one binary format that embeds outward text references (material paths).
static std::string build_glob_excludes() {
	std::string out;
	for (auto& ext :
		 {"dds", "wav", "png", "jpg","tga", "jpeg", "hdr", "glb", "blend", "ssbar", "psd", "fbx", "zip", "exr", "gltf"})
		out += " --glob \"!*." + std::string(ext) + "\"";
	return out;
}

static std::string build_extension_alternation() {
	std::string out;
	for (auto& ext : get_all_known_extensions()) {
		if (!out.empty())
			out += "|";
		out += ext;
	}
	return out;
}

// rg -o output is the raw matched token (e.g. an embedded path string with no
// surrounding quotes). Resolve it to a real registered game path: try a direct
// match first, then fall back to the longest known asset path that the token
// ends with (handles tokens captured with extra leading path components).
static std::string resolve_to_known_asset(std::string token, const std::unordered_set<std::string>& known) {
	normalize_slashes(token);
	if (known.count(token))
		return token;
	std::string best;
	for (auto& gp : known) {
		if (gp.size() > token.size())
			continue;
		if (token.compare(token.size() - gp.size(), gp.size(), gp) != 0)
			continue;
		// require a path-boundary before the match (start of string or a '/')
		size_t start = token.size() - gp.size();
		if (start != 0 && token[start - 1] != '/')
			continue;
		if (gp.size() > best.size())
			best = gp;
	}
	return best;
}

namespace AssetReferenceQuery
{

std::vector<AssetRefHit> find_backward_references(const std::string& asset_gamepath) {
	std::vector<AssetRefHit> hits;

	std::string dataDir = FileSys::get_game_path();
	std::string args = "-a --count -F -e \"" + asset_gamepath + "\"" + build_glob_excludes() + " \"" + dataDir + "\"";
	std::string output = run_ripgrep_captured(args);

	auto& reg = AssetRegistrySystem::get();
	for (auto& line : split_lines(output)) {
		auto colon = line.rfind(':');
		if (colon == std::string::npos)
			continue;
		std::string fullPath = line.substr(0, colon);
		int count = atoi(line.substr(colon + 1).c_str());
		normalize_slashes(fullPath);

		std::string gamedir = FileSys::get_game_path();
		normalize_slashes(gamedir);
		std::string gp = FileSys::get_game_path_from_full_path(fullPath);
		normalize_slashes(gp);
		if (gp == asset_gamepath)
			continue; // don't report the asset referencing its own path text

		hits.push_back({gp, reg.find_metadata_for_ext(get_extension_no_dot(gp)), count});
	}
	return hits;
}

std::vector<AssetRefHit> find_forward_references(const std::string& asset_gamepath) {
	std::vector<AssetRefHit> hits;

	std::string fullPath = FileSys::get_full_path_from_game_path(asset_gamepath);
	std::string pattern = "[A-Za-z0-9_.\\-/\\\\]+\\.(" + build_extension_alternation() + ")\\b";
	std::string args = "-a -o -N -e \"" + pattern + "\" \"" + fullPath + "\"";
	std::string output = run_ripgrep_captured(args);

	std::unordered_set<std::string> known;
	for (auto* node : AssetRegistrySystem::get().get_linear_list())
		known.insert(node->asset.filename);

	std::unordered_map<std::string, int> tally;
	for (auto& line : split_lines(output)) {
		std::string resolved = resolve_to_known_asset(line, known);
		if (!resolved.empty() && resolved != asset_gamepath)
			tally[resolved]++;
	}

	auto& reg = AssetRegistrySystem::get();
	for (auto& [gp, count] : tally)
		hits.push_back({gp, reg.find_metadata_for_ext(get_extension_no_dot(gp)), count});
	return hits;
}

std::vector<AssetRefHit> find_transitive_references(const std::string& asset_gamepath, bool backward, int max_depth) {
	std::vector<AssetRefHit> flat;
	std::unordered_set<std::string> visited{asset_gamepath};
	std::deque<std::pair<std::string, int>> queue{{asset_gamepath, 0}};

	while (!queue.empty()) {
		auto [gp, depth] = queue.front();
		queue.pop_front();
		if (depth >= max_depth)
			continue;
		// Leaf binaries (textures/audio/source meshes) never contain outward
		// references - skip the query instead of spawning rg just to learn that.
		if (!backward && is_leaf_binary_ext(get_extension_no_dot(gp)))
			continue;

		auto direct = backward ? find_backward_references(gp) : find_forward_references(gp);
		for (auto& hit : direct) {
			if (visited.insert(hit.game_path).second) {
				flat.push_back(hit);
				queue.push_back({hit.game_path, depth + 1});
			}
		}
	}
	return flat;
}

}

#endif
