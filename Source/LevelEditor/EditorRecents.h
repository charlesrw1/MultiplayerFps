#pragma once
#ifdef EDITOR_BUILD
#include "LevelEditorCamera.h"
#include <deque>
#include <optional>
#include <string>

class Cmd_Args;

// Persistent history of recently-opened editor documents plus their last-known
// camera pose. Populated by GameEngineLocal::open_tool when switching docs and
// surfaced via the `recent` console command.
//
// Slots are 1-indexed for user-facing commands; slot 1 is the most recent
// document the user switched AWAY from (the current doc is NOT in the list).
struct RecentDocEntry
{
	std::string path;
	CameraSnapshot camera;
};

class EditorRecents
{
public:
	static constexpr int MAX_ENTRIES = 10;

	// Move-to-front insert with dedup-by-path. Caps deque to MAX_ENTRIES.
	// Persists on every call. No-op if path is empty.
	void record(std::string path, const CameraSnapshot& cam);

	// 1-indexed lookup. Returns nullopt if slot is out of range.
	std::optional<RecentDocEntry> at_slot(int one_indexed_slot) const;

	// Prints numbered list via args.sys_print, or a "no recent documents"
	// notice if the deque is empty.
	void print_list(const Cmd_Args& args) const;

	// Reads Data/editor_recents.json. Silent if the file doesn't exist.
	// Tolerant of malformed JSON: logs a warning and leaves entries empty.
	void load();

	// Overwrites Data/editor_recents.json with the current entries.
	void save() const;

	int size() const { return (int)entries.size(); }

private:
	std::deque<RecentDocEntry> entries;
};

extern EditorRecents g_editor_recents;
#endif
