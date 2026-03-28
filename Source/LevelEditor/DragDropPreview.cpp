#include "DragDropPreview.h"

#include "Game/Components/MeshComponent.h"
#include "Render/MaterialPublic.h"
void DragDropPreview::set_preview_model(Model* m, const glm::mat4& where) {

	had_state_set = true;
	if (!(state == State::PreviewModel && preview_model == m)) {
		state = State::PreviewModel;
		preview_model = m;
		delete_obj();
		Entity* e = eng->get_level()->spawn_entity();
		e->dont_serialize_or_edit = true;
		auto mesh = e->create_component<MeshComponent>();
		mesh->set_model(m);
		mesh->set_material_override(MaterialInstance::load("trigger_zone.mm"));
		obj_ptr = e;
		fixup_entity();
	}
	if (obj_ptr) {
		obj_ptr->set_ws_transform(where);
	}
}

void DragDropPreview::set_preview_component(const ClassTypeInfo* t, const glm::mat4& where) {

	if (!t || !t->is_a(Component::StaticType)) {
		sys_print(Warning, "set_preview_component: not a Component subtype\n");
		return;
	}
	had_state_set = true;
	if (!(state == State::PreviewComponent && preview_comp == t)) {
		state = State::PreviewComponent;
		preview_comp = t;
		delete_obj();
		Entity* e = eng->get_level()->spawn_entity();
		e->dont_serialize_or_edit = true;
		e->create_component(t);
		obj_ptr = e;
		fixup_entity();
	}
	if (obj_ptr) {
		obj_ptr->set_ws_transform(where);
	}
}

void DragDropPreview::tick() {
	if (!had_state_set) {
		delete_obj();
		state = State::None;
		assert(!obj_ptr);
	}
	else {
		had_state_set = false;
	}
}
#include "Game/Components/ArrowComponent.h"
#include "Game/Components/BillboardComponent.h"
void DragDropPreview::fixup_entity() {
	auto set_r = [](auto&& self, Entity* e) -> void {
		for (Component* c : e->get_components()) {
			if (auto mesh = c->cast_to<MeshComponent>()) {
				mesh->set_material_override(MaterialInstance::load("trigger_zone.mm"));
			}
			if (auto arrow = c->cast_to<ArrowComponent>()) {
				arrow->visible = false;
			}
			if (auto bb = c->cast_to<BillboardComponent>()) {
				bb->set_is_visible(false);
			}
		}
		for (Entity* c : e->get_children()) {
			self(self, c);
		}
	};
	if (obj_ptr)
		set_r(set_r, obj_ptr.get());
}

void DragDropPreview::delete_obj() {
	Entity* e = obj_ptr.get();
	if (e) {
		e->destroy_deferred();
	}
	obj_ptr = nullptr;
}