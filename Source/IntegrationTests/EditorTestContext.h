// Source/IntegrationTests/EditorTestContext.h
#pragma once
#ifdef EDITOR_BUILD
#include <string>

// Thin wrapper giving integration tests access to editor-specific APIs.
// Obtained via TestContext::editor() — only valid during EDITOR_TEST runs.
class EditorTestContext
{
public:
	// Number of entities currently in the level.
	int entity_count() const;

	// Save the current level to a game-relative path (e.g. "TestFiles/tmp.tmap").
	void save_level(const char* path);

	// Invoke undo on the editor's command manager.
	void undo();
};
#endif
