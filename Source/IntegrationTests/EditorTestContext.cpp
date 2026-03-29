// Source/IntegrationTests/EditorTestContext.cpp
#ifdef EDITOR_BUILD
#include "EditorTestContext.h"
#include "GameEngineLocal.h"
#include "LevelEditor/EditorDocLocal.h"
#include "Level.h"
#include <cassert>

static EditorDoc* get_doc() {
	auto* tool = eng->get_tool();
	assert(tool && "editor tool is null — not in editor mode?");
	auto* doc = static_cast<EditorDoc*>(tool);
	assert(doc && "editor tool is not EditorDoc");
	return doc;
}

int EditorTestContext::entity_count() const {
	return (int)eng->get_level()->get_all_objects().num_used;
}

void EditorTestContext::save_level(const char* path) {
	EditorDoc* doc = get_doc();
	doc->set_document_path(path);
	doc->save_document_internal();
}

void EditorTestContext::undo() {
	EditorDoc* doc = get_doc();
	doc->command_mgr->undo();
}
#endif
