#pragma once
#ifdef EDITOR_BUILD
#include "Assets/AssetDatabase.h"
#include "Game/Components/MeshComponent.h"
#include <memory>
#include "Game/BaseUpdater.h"
#include "Game/EntityPtr.h"
#include "Game/Entity.h"
#include <string>
#include <vector>
#include "Framework/Util.h"
#include "GameEnginePublic.h"
#include <stdexcept>
#include "Framework/MulticastDelegate.h"
#include <functional>
#include "LevelSerialization/SerializeNew.h"
#include <unordered_map>
class Command
{
public:
	virtual ~Command() { sys_print(Debug, "~Command\n"); }
	virtual void execute() = 0;
	virtual void undo() = 0;
	virtual std::string to_string() = 0;
	virtual bool is_valid() { return true; }
};

struct SDL_KeyboardEvent;
class UndoRedoSystem
{
public:
	UndoRedoSystem();
	~UndoRedoSystem() {
		clear_all();
		for (auto& c : queued_commands)
			delete c.c;
		queued_commands.clear();
	}

	void on_key_event(const SDL_KeyboardEvent& k);

	void clear_all();
	// returns number of errord commands
	int execute_queued_commands();

	void add_command(Command* c) {
		queued_commands.push_back({ c });
	}
	void add_command_with_execute_callback(Command* c, std::function<void(bool)> callback) {
		queued_commands.push_back({ c, callback });
	}

	void undo();


	MulticastDelegate<> on_command_execute_or_undo;
	struct Queued {
		Command* c = nullptr;
		std::function<void(bool)> func;
	};
	std::vector<Queued> queued_commands;

	const int HIST_SIZE = 128;
	int index = 0;
	std::vector<Command*> hist;
};

class CommandSerializeUtil
{
public:
	static std::unique_ptr<SerializedSceneFile> serialize_entities_text(EditorDoc& ed_doc, std::vector<EntityPtr> handles);
};

class EditorDoc;
struct SavedCreateObj {
	uint64_t eng_handle = 0;
};
class RemoveEntitiesCommand : public Command
{
public:
	std::vector<SavedCreateObj> removed_objs;

	EditorDoc& ed_doc;
	RemoveEntitiesCommand(EditorDoc& ed_doc, std::vector<EntityPtr> handles);
	bool is_valid_flag = true;
	bool is_valid() final {
		return is_valid_flag;
	}

	void execute() final;
	void undo() final;
	std::string to_string() final {
		return "Remove Entity";
	}

	std::unique_ptr<SerializedSceneFile> scene;
	std::vector<EntityPtr> handles;
};


class CreateStaticMeshCommand : public Command
{
public:
	EditorDoc& ed_doc;
	CreateStaticMeshCommand(EditorDoc& ed_doc, const std::string& modelname, const glm::mat4& transform, EntityPtr parent = EntityPtr());
	~CreateStaticMeshCommand() override {
	}

	bool is_valid() final { return true; }

	void execute() final;
	void undo() final;
	std::string to_string() final {
		return "Create StaticMesh";
	}
	EntityPtr parent_to;
	EntityPtr handle;
	glm::mat4 transform;
	std::string modelname{};
};
class CreateCppClassCommand : public Command
{
public:
	EditorDoc& ed_doc;
	CreateCppClassCommand(EditorDoc& ed_doc, const std::string& cppclassname, const glm::mat4& transform, EntityPtr parent, bool is_component);
	bool is_valid() final { return ti != nullptr; }

	void execute() final;
	void undo() final;
	std::string to_string() final {
		return "Create Class";
	}
	const ClassTypeInfo* ti = nullptr;
	glm::mat4 transform;
	EntityPtr handle;
	EntityPtr parent_to;
	bool is_component_type = false;
};
class CreateSpawnerCommand : public Command
{
public:
	EditorDoc& ed_doc;
	CreateSpawnerCommand(EditorDoc& ed_doc, const std::string& cppclassname, const glm::mat4& transform);

	void execute() final;
	void undo() final;
	std::string to_string() final {
		return "Create Spawner";
	}
	std::string cppclassname;
	glm::mat4 transform;
	EntityPtr handle;
};


class TransformCommand : public Command
{
public:
	EditorDoc& ed_doc;
	TransformCommand(EditorDoc& ed_doc, const std::unordered_set<uint64_t>& selection, const std::unordered_map<uint64_t, glm::mat4>& pre_transforms);
	void execute() final;
	void undo() final;
	std::string to_string() final {
		return "Transform Entities";
	}
	bool skip_this_time = false;
	struct pre_and_post {
		EntityPtr ptr;
		glm::mat4 pre_transform;
		glm::mat4 post_transform;
	};
	std::vector<pre_and_post> transforms;
};


class DuplicateEntitiesCommand : public Command
{
public:
	EditorDoc& ed_doc;
	DuplicateEntitiesCommand(EditorDoc& ed_doc, std::vector<EntityPtr> handles);
	bool is_valid_flag = true;
	bool is_valid() final {
		return is_valid_flag;
	}

	void execute() final;
	void undo() final;
	std::string to_string() final {
		return "Duplicate Entity";
	}

	std::unique_ptr<SerializedSceneFile> scene;
	std::vector<EntityPtr> handles;
};

class MovePositionInHierarchy : public Command
{
public:
	enum class Cmd {
		Next,
		Prev,
		First,
		Last
	};
	EditorDoc& ed_doc;
	MovePositionInHierarchy(EditorDoc& ed_doc, Entity* e, Cmd cmd);
	bool is_valid() final {
		return entPtr;
	}
	void execute() final;
	void undo() final;
	std::string to_string() final {
		return "MovePositionInHeirarchy";
	}
	int to_position = 0;
	int from_position = 0;
	EntityPtr entPtr{};
};

class CreateComponentCommand : public Command
{
public:
	EditorDoc& ed_doc;
	CreateComponentCommand(EditorDoc& ed_doc, Entity* e, const ClassTypeInfo* component_type) :ed_doc(ed_doc){
		ent = e->get_self_ptr();
		ASSERT(component_type->is_a(Component::StaticType));
		info = component_type;
	}
	void execute() final;
	virtual void post_create(Component* ec);
	void undo() final;
	std::string to_string() override {
		return "Create Component";
	}
	EntityPtr ent;
	uint64_t comp_handle = 0;
	const ClassTypeInfo* info = nullptr;
};

class CreateMeshComponentCommand : public CreateComponentCommand
{
public:
	CreateMeshComponentCommand(EditorDoc& ed_doc, Entity* e, Model* s)
		: CreateComponentCommand(ed_doc, e, &MeshComponent::StaticType) {
		this->s = s;
	}
	void post_create(Component* ec)override {
		auto sc = ec->cast_to<MeshComponent>();
		ASSERT(sc);
		sc->set_model(s);
	}

	Model* s = nullptr;
};

class CreateEntityCommand : public Command
{
public:
	CreateEntityCommand(EditorDoc& ed_doc, std::function<void(Entity*)> post_create) : doc(doc),post_create(post_create) {
	}
	std::string to_string() override {
		return "CreateEntityCommand";
	}
	void undo() final {
		auto obj = eng->get_object(ptr);
		if (obj)ptr->destroy();
		obj = EntityPtr();
	}
	void execute() final;

	std::function<void(Entity*)> post_create;
	EditorDoc& doc;
	EntityPtr ptr;
};


class RemoveComponentCommand  : public Command
{
public:
	EditorDoc& ed_doc;
	RemoveComponentCommand(EditorDoc& ed_doc, Entity* e, Component* which) : ed_doc(ed_doc){
		ent = e->get_self_ptr();
		comp_handle = which->get_instance_id();
		info = &which->get_type();
	}
	void execute() final;
	void undo() final;
	std::string to_string() final {
		return "Remove Component";
	}
	EntityPtr ent;
	uint64_t comp_handle = 0;
	const ClassTypeInfo* info = nullptr;
};

class SetFolderCommand : public Command {
public:
	EditorDoc& ed_doc;
	SetFolderCommand(EditorDoc& ed_doc, std::vector<EntityPtr> ptrs, int folderId) : ed_doc(ed_doc) {
		this->ptrs = ptrs;
		this->setTo = folderId;
	}
	void execute() final {
		prevIds.clear();
		prevIds.resize(ptrs.size());
		for (int i = 0; i < ptrs.size();i++) {
			auto ent = ptrs[i].get();
			if (ent) {
				prevIds.at(i) = ent->get_folder_id();
				ent->set_folder_id(setTo);
			}
		}
	}
	void undo() final {
		for (int i = 0; i < ptrs.size(); i++) {
			auto ent = ptrs[i].get();
			if (ent) {
				ent->set_folder_id(prevIds.at(i));
			}
		}
	}
	std::string to_string() final {
		return "SetFolderCommand";
	}
	std::vector<EntityPtr> ptrs;
	std::vector<int8_t> prevIds;
	int setTo = 0;
};

#endif