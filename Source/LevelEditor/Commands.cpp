#ifdef EDITOR_BUILD
#include "Commands.h"
#include <unordered_set>
#include "Framework/MapUtil.h"
#include "EditorDocLocal.h"
#include "Game/Prefab.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "Framework/Log.h"
#include "LevelSerialization/SerializeNew.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void validate_remove_entities(EditorDoc& ed_doc, std::vector<EntityPtr>& input) {
	ASSERT(true); // input may be empty; errors are logged below
	bool had_errors = false;
	for (int i = 0; i < (int)input.size(); i++) {
		auto e = input[i];
		Entity* ent = e.get();
		if (!ent)
			continue; // whatever, doesnt matter

		if (!ed_doc.can_delete_this_object(ent)) {
			had_errors = true;
			input.erase(input.begin() + i);
			i--;
		}
	}
	if (had_errors)
		eng->log_to_fullscreen_gui(Error, "Cant remove inherited entities");
}

static void add_to_remove_list_R(vector<SavedCreateObj>& objs, Entity* e, std::unordered_set<BaseUpdater*>& seen) {
	ASSERT(e != nullptr);
	if (!this_is_a_serializeable_object(e))
		return;
	if (SetUtil::contains(seen, (BaseUpdater*)e))
		return;
	SetUtil::insert_test_exists(seen, (BaseUpdater*)e);

	for (auto c : e->get_components()) {
		if (!this_is_a_serializeable_object(c))
			continue;
		if (SetUtil::contains(seen, (BaseUpdater*)c))
			continue;
		SetUtil::insert_test_exists(seen, (BaseUpdater*)c);

		SavedCreateObj created;
		created.eng_handle = c->get_instance_id();
		objs.push_back(created);
	}
	SavedCreateObj created;
	created.eng_handle = e->get_instance_id();

	objs.push_back(created);
	for (auto c : e->get_children()) {
		add_to_remove_list_R(objs, c, seen);
	}
}

// ---------------------------------------------------------------------------
// RemoveEntitiesCommand
// ---------------------------------------------------------------------------

RemoveEntitiesCommand::RemoveEntitiesCommand(EditorDoc& ed_doc, std::vector<EntityPtr> handles) : ed_doc(ed_doc) {
	ASSERT(!handles.empty() || true); // validity checked below
	validate_remove_entities(ed_doc, handles);
	for (EntityPtr e : handles) {
		is_valid_flag &= ed_doc.can_delete_this_object(e.get());
	}
	if (handles.empty())
		is_valid_flag = false;
	if (!is_valid_flag)
		return;

	ed_doc.validate_fileids_before_serialize();
	std::unordered_set<BaseUpdater*> seen;
	for (EntityPtr e : handles) {
		Entity* ent = e.get();
		if (ent)
			add_to_remove_list_R(removed_objs, ent, seen);
		else {
			sys_print(Warning, "RemoveEntitiesCommand(): handle invalid: %lld\n", e.handle);
		}
	}

	scene = CommandSerializeUtil::serialize_entities_text(ed_doc, handles);
	assert(seen.size() == removed_objs.size());

	this->handles = handles;
}

void RemoveEntitiesCommand::execute() {
	ASSERT(is_valid());
	for (auto h : handles) {
		h->destroy();
	}
	ed_doc.post_node_changes.invoke();
}

void RemoveEntitiesCommand::undo() {
	ASSERT(is_valid());

	auto restored = NewSerialization::unserialize_from_text("remove_entities_undo", scene->text, true /* restore id*/);

	ed_doc.insert_unserialized_into_scene(restored);

	for (SavedCreateObj c : removed_objs) {
		BaseUpdater* obj = eng->get_level()->get_entity(c.eng_handle);
		if (!obj) {
			sys_print(Warning, "RemoveEntitiesCommand::undo: object cant be found to put back %lld\n", c.eng_handle);
			continue;
		}

		assert(obj->get_instance_id() == c.eng_handle);
	}

	// refresh handles i guess ? fixme
	handles.clear();

	ed_doc.post_node_changes.invoke();
}

// ---------------------------------------------------------------------------
// CreateStaticMeshCommand
// ---------------------------------------------------------------------------

CreateStaticMeshCommand::CreateStaticMeshCommand(EditorDoc& ed_doc, const std::string& modelname,
												 const glm::mat4& transform, EntityPtr parent)
	: ed_doc(ed_doc) {
	ASSERT(!modelname.empty());
	this->transform = transform;
	this->modelname = modelname;
	this->parent_to = parent;
}

void CreateStaticMeshCommand::execute() {
	ASSERT(!modelname.empty());
	auto ent = ed_doc.spawn_entity();
	ed_doc.attach_component(&MeshComponent::StaticType, ent);
	if (parent_to.get())
		ent->parent_to(parent_to.get());
	else
		ent->set_ws_transform(transform);

	handle = ent->get_self_ptr();

	ed_doc.selection_state->set_select_only_this(ent->get_self_ptr());

	ed_doc.on_entity_created.invoke(handle);
	ed_doc.post_node_changes.invoke();

	Model* modelP = g_assets.find<Model>(modelname).get();
	if (modelP) {
		if (ent) {
			auto mesh_ent = ent->cast_to<Entity>();
			ASSERT(mesh_ent);
			auto firstmesh = mesh_ent->get_component<MeshComponent>();
			if (firstmesh)
				firstmesh->set_model(modelP);
			else
				sys_print(Warning, "CreateStaticMeshCommand couldnt find mesh component\n");
		} else
			sys_print(Warning, "CreateStaticMeshCommand: ent handle invalid in async callback\n");
	}
}

void CreateStaticMeshCommand::undo() {
	ASSERT(handle.get() != nullptr);
	handle->destroy();
	ed_doc.post_node_changes.invoke();
	handle = {};
}

// ---------------------------------------------------------------------------
// CreateCppClassCommand
// ---------------------------------------------------------------------------

CreateCppClassCommand::CreateCppClassCommand(EditorDoc& ed_doc, const std::string& cppclassname,
											 const glm::mat4& transform, EntityPtr parent, bool is_component)
	: ed_doc(ed_doc) {
	ASSERT(!cppclassname.empty());
	auto find = cppclassname.rfind('/');
	auto types = cppclassname.substr(find == std::string::npos ? 0 : find + 1);
	ti = ClassBase::find_class(types.c_str());
	this->transform = transform;
	this->parent_to = parent;
	is_component_type = is_component;
}

void CreateCppClassCommand::execute() {
	ASSERT(ti != nullptr);
	Entity* ent{};
	if (is_component_type) {
		ent = ed_doc.spawn_entity();
		ed_doc.attach_component(ti, ent);
	} else
		ent = ed_doc.spawn_entity();

	if (parent_to.get())
		ent->parent_to(parent_to.get());

	ent->set_ws_transform(transform);

	ASSERT(!ent->dont_serialize_or_edit);

	handle = ent->get_self_ptr();
	ed_doc.selection_state->set_select_only_this(ent->get_self_ptr());
	ed_doc.on_entity_created.invoke(handle);
	ed_doc.post_node_changes.invoke();
}

void CreateCppClassCommand::undo() {
	ASSERT(handle.get() != nullptr);
	auto ent = handle.get();
	ed_doc.remove_scene_object(ent);
	if (ed_doc.selection_state->is_entity_selected(ent)) {
		ed_doc.selection_state->clear_all_selected();
	}
	ed_doc.post_node_changes.invoke();
	handle = {};
}

// ---------------------------------------------------------------------------
// TransformCommand
// ---------------------------------------------------------------------------

TransformCommand::TransformCommand(EditorDoc& ed_doc, const std::unordered_set<uint64_t>& selection,
								   const std::unordered_map<uint64_t, glm::mat4>& pre_transforms)
	: ed_doc(ed_doc) {
	ASSERT(!selection.empty() || true); // empty selection is legal but produces no-op command
	for (auto& pair : selection) {
		auto find = pre_transforms.find(pair);
		if (find != pre_transforms.end()) {

			EntityPtr e(find->first);
			if (e.get()) {
				pre_and_post pp;
				pp.ptr = e;
				pp.pre_transform = find->second;
				pp.post_transform = e->get_ws_transform();
				transforms.push_back(pp);
			}
		}
	}
	skip_this_time = true;
}

void TransformCommand::execute() {
	ASSERT(true); // transforms may be empty
	// already have ws transforms
	if (skip_this_time) {
		skip_this_time = false;
		return;
	}
	for (auto& t : transforms) {
		if (t.ptr.get()) {
			t.ptr->set_ws_transform(t.post_transform);
		}
	}
	ed_doc.selection_state->on_selection_changed.invoke(); // hack
}

void TransformCommand::undo() {
	ASSERT(true); // transforms may be empty
	for (auto& t : transforms) {
		if (t.ptr.get()) {
			t.ptr->set_ws_transform(t.pre_transform);
		}
	}
	ed_doc.selection_state->on_selection_changed.invoke(); // hack
}

// ---------------------------------------------------------------------------
// DuplicateEntitiesCommand
// ---------------------------------------------------------------------------

DuplicateEntitiesCommand::DuplicateEntitiesCommand(EditorDoc& ed_doc, std::vector<EntityPtr> handles) : ed_doc(ed_doc) {
	ASSERT(!handles.empty() || true); // validity checked below
	if (handles.empty())
		is_valid_flag = false;

	if (!is_valid_flag)
		return;

	scene = CommandSerializeUtil::serialize_entities_text(ed_doc, handles);
}

void DuplicateEntitiesCommand::execute() {
	ASSERT(is_valid());
	UnserializedSceneFile duplicated =
		NewSerialization::unserialize_from_text("duplicate_entities", scene->text, false /* dont keep id*/);

	ed_doc.insert_unserialized_into_scene(duplicated);

	ed_doc.selection_state->clear_all_selected();

	vector<EntityPtr> ents;
	for (auto e : duplicated.all_obj_vec)
		if (auto ent = e->cast_to<Entity>()) {
			ents.push_back(ent);
			handles.push_back(ent);
		}
	ed_doc.selection_state->add_entities_to_selection(ents);

	ed_doc.manipulate->set_force_op(ImGuizmo::TRANSLATE);
	ed_doc.manipulate->set_force_gizmo_on(true);

	ed_doc.post_node_changes.invoke();
}

void DuplicateEntitiesCommand::undo() {
	ASSERT(true); // handles may be empty if execute failed
	for (auto h : handles) {
		h->destroy();
	}

	ed_doc.post_node_changes.invoke();
}

// ---------------------------------------------------------------------------
// MovePositionInHierarchy
// ---------------------------------------------------------------------------

MovePositionInHierarchy::MovePositionInHierarchy(EditorDoc& ed_doc, Entity* e, Cmd cmd) : ed_doc(ed_doc) {
	ASSERT(e != nullptr || true); // null entity is handled gracefully
	if (!e)
		return;
	const auto parent = e->get_parent();
	if (!parent)
		return;
	from_position = parent->get_child_entity_index(e);
	if (from_position == -1)
		return;
	auto& children = parent->get_children();
	const int last_idx = (int)children.size() - 1;
	switch (cmd) {
	case Cmd::Next:
		to_position = std::min((from_position + 1), last_idx);
		break;
	case Cmd::Prev:
		to_position = std::max((from_position - 1), 0);
		break;
	case Cmd::Last:
		to_position = last_idx;
		break;
	case Cmd::First:
		to_position = 0;
		break;
	}

	entPtr = e->get_self_ptr();
}

void MovePositionInHierarchy::execute() {
	ASSERT(entPtr || true); // is_valid() guards external callers
	auto e = entPtr.get();
	if (!e || !e->get_parent())
		return;
	e->get_parent()->move_child_entity_index(e, to_position);

	ed_doc.post_node_changes.invoke();
}

void MovePositionInHierarchy::undo() {
	ASSERT(entPtr || true); // is_valid() guards external callers
	auto e = entPtr.get();
	if (!e || !e->get_parent())
		return;
	e->get_parent()->move_child_entity_index(e, from_position);

	ed_doc.post_node_changes.invoke();
}

// ---------------------------------------------------------------------------
// CommandSerializeUtil
// ---------------------------------------------------------------------------

std::unique_ptr<SerializedSceneFile> CommandSerializeUtil::serialize_entities_text(EditorDoc& ed_doc,
																				   std::vector<EntityPtr> handles) {
	ASSERT(!handles.empty());
	std::vector<Entity*> ents;
	for (auto h : handles) {
		ents.push_back(h.get());
	}
	ed_doc.validate_fileids_before_serialize();

	return std::make_unique<SerializedSceneFile>(
		NewSerialization::serialize_to_text("Command::serialize_entities_text", ents, true));
}
#endif
