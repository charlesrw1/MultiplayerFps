#ifdef EDITOR_BUILD
#include "Commands.h"
#include "EditorDocLocal.h"
#include "GameEnginePublic.h"
#include "Framework/Log.h"

void RemoveComponentCommand::execute() {
	ASSERT(comp_handle != 0);
	auto obj = eng->get_object(comp_handle);
	ASSERT(obj->is_a<Component>());
	auto ec = (Component*)obj;
	auto id = ec->get_instance_id();
	ec->destroy();
	// dont move this!
	ed_doc.on_component_deleted.invoke(id);
	comp_handle = 0;
}

void RemoveComponentCommand::undo() {
	ASSERT(comp_handle == 0);
	auto e = ent.get();
	if (!e) {
		sys_print(Warning, "no entity in RemoveComponentCommand\n");
		return;
	}
	auto ec = e->create_component(info);
	comp_handle = ec->get_instance_id();
}

void CreateComponentCommand::execute() {
	ASSERT(comp_handle == 0);
	auto e = ent.get();
	if (!e) {
		sys_print(Warning, "no entity in createcomponentcommand\n");
		return;
	}
	auto ec = ed_doc.attach_component(info, e);
	comp_handle = ec->get_instance_id();
	post_create(ec);

	ed_doc.on_component_created.invoke(ec);
}

void CreateComponentCommand::post_create(Component* ec) {}

void CreateComponentCommand::undo() {
	ASSERT(comp_handle != 0);
	auto obj = eng->get_object(comp_handle);
	ASSERT(obj->is_a<Component>());
	auto ec = (Component*)obj;
	auto id = ec->get_instance_id();
	ec->destroy();
	ed_doc.on_component_deleted.invoke(id);
	comp_handle = 0;
}

void CreateEntityCommand::execute() {
	ASSERT(post_create != nullptr);
	Entity* e = doc.spawn_entity();
	ptr = e;
	post_create(e);
}
#endif
