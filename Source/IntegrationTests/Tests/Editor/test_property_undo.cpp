// Source/IntegrationTests/Tests/Editor/test_property_undo.cpp
// Property-edit dirty-flag + undo (SetEntityStateCommand) and the backup writer/restore path.
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/EditorTestContext.h"
#include "GameEnginePublic.h"
#include "LevelEditor/EditorDocLocal.h"
#include "LevelEditor/Commands.h"
#include "LevelSerialization/SerializeNew.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Framework/Files.h"

// Regression for a crash where SetEntityStateCommand::apply fed the raw "!json\n{...}"
// snapshot text (as produced by CommandSerializeUtil::serialize_entities_text) straight into
// nlohmann::json::parse, which throws on the "!json" prefix and was never caught — any
// property-grid edit (even a single checkbox click) crashed the editor on commit.
static TestTask test_property_edit_command_undo_redo(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	Entity* e = editor->spawn_entity();
	e->set_editor_name("PropUndoTest");
	MeshComponent* mc = e->create_component<MeshComponent>();
	mc->set_is_visible(true);
	co_await t.wait_ticks(1);

	EntityPtr eptr = e->get_self_ptr();
	std::shared_ptr<SerializedSceneFile> before(CommandSerializeUtil::serialize_entities_text(*editor, {eptr}));
	t.check(before->text.rfind("!json", 0) == 0, "snapshot carries the !json marker prefix");

	// Simulate what a live property-grid checkbox edit does: mutate the value in place,
	// then snapshot "after" the same way EdPropertyGrid does on session-end.
	mc->set_is_visible(false);
	std::shared_ptr<SerializedSceneFile> after(CommandSerializeUtil::serialize_entities_text(*editor, {eptr}));

	SetEntityStateCommand cmd(*editor, eptr, before, after);

	// Must not throw/crash parsing the marker-prefixed snapshot text (the actual regression).
	cmd.execute();
	t.check(mc->get_is_visible() == false, "value holds after (redundant) execute() re-apply");

	cmd.undo();
	t.check(mc->get_is_visible() == true, "undo restores prior value");

	cmd.execute();
	t.check(mc->get_is_visible() == false, "redo re-applies value");

	e->destroy();
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/property_edit_command_undo_redo", 20.f, test_property_edit_command_undo_redo);

// Same crash, reached through the real path: EditorDoc::tick()'s backup writer produces
// files with the same "!json" prefix, and restoring one goes through
// EditorDoc::replace_level_content_from_text -> NewSerialization::unserialize_from_text,
// which already stripped the marker correctly (unlike the bug above) but is worth covering
// end to end since RestoreBackupCommand shares the same "!json"-prefixed snapshot format.
static TestTask test_backup_write_and_restore(TestContext& t) {
	const std::string mapPath = "_property_undo_backup_test.tmap";
	FileSys::delete_game_file(mapPath.c_str());
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, ("open-editor " + mapPath).c_str());
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");
	editor->set_document_path(mapPath);

	Entity* e = editor->spawn_entity();
	e->set_editor_name("BackupTest");
	e->create_component<MeshComponent>();
	co_await t.wait_ticks(1);

	editor->set_has_editor_changes();
	t.check(editor->get_has_editor_changes(), "doc marked dirty before backup");

	const std::string backupDir = editor->backup_dir_for_current_asset();
	FileSys::create_directory(backupDir, FileSys::GAME_DIR);
	editor->write_backup();

	auto backups = editor->list_backups_for_current_asset();
	t.require(!backups.empty(), "backup file written");

	auto file = FileSys::open_read_game(backups.back());
	t.require(file != nullptr, "backup file opens for read");
	std::string text(file->size(), ' ');
	file->read((void*)text.data(), text.size());
	t.check(text.rfind("!json", 0) == 0, "backup text carries the !json marker prefix");

	// Destroy the entity, then restore from the backup and confirm it comes back.
	e->destroy();
	co_await t.wait_ticks(1);
	t.check(nullptr == [&]() -> Entity* {
		for (auto o : eng->get_level()->get_all_objects())
			if (auto ent = o->cast_to<Entity>())
				if (ent->get_editor_name() == "BackupTest")
					return ent;
		return nullptr;
	}(), "entity gone before restore");

	RestoreBackupCommand restore(*editor, std::move(text));
	restore.execute(); // must not throw/crash parsing the marker-prefixed backup text
	co_await t.wait_ticks(1);

	Entity* restored = nullptr;
	for (auto o : eng->get_level()->get_all_objects())
		if (auto ent = o->cast_to<Entity>())
			if (ent->get_editor_name() == "BackupTest")
				restored = ent;
	t.check(restored != nullptr, "entity restored from backup");

	FileSys::delete_game_file(mapPath.c_str());
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/backup_write_and_restore", 20.f, test_backup_write_and_restore);
