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

#include "Game/Prefab.h"
#include "Game/Components/LightComponents.h"
#include "Framework/SerializerJson2.h"
#include "Assets/AssetDatabase.h"
namespace {
bool is_prefab_ghost_component(const Component* c) {
	return c && (c->is_a<MeshComponent>() || c->is_a<SpotLightComponent>() || c->is_a<PointLightComponent>() ||
				 c->is_a<SunLightComponent>() || c->is_a<SkylightComponent>() || c->is_a<AreaishLightComponent>());
}
// Copies `src`'s REF fields onto `dst` (same type) via an in-memory JSON round trip, reusing
// the engine's existing generic reflection serializers instead of per-component-type copy code.
void copy_component_fields(Component* dst, Component* src) {
	WriteSerializerBackendJson2 writer("prefab_ghost_copy", *src);
	nlohmann::json data = writer.get_output();
	ReadSerializerBackendJson2 reader("prefab_ghost_copy", data, *dst);
}
// Composes an entity's transform up through its (unattached, prefab-local) parent chain --
// this is the entity's placement within the prefab, independent of any Level.
glm::mat4 composed_prefab_local_transform(Entity* e) {
	glm::mat4 m = e->get_ls_transform();
	for (Entity* p = e->get_parent(); p; p = p->get_parent())
		m = p->get_ls_transform() * m;
	return m;
}
} // namespace

void DragDropPreview::set_preview_prefab(const std::string& path, const glm::mat4& where) {
	had_state_set = true;
	if (!(state == State::PreviewPrefab && preview_prefab_path == path)) {
		state = State::PreviewPrefab;
		preview_prefab_path = path;
		delete_obj();

		auto asset = g_assets.find<PrefabAsset>(path);
		if (!asset)
			return;

		UnserializedSceneFile scratch;
		try {
			scratch = NewSerialization::unserialize_from_text("prefab_ghost_preview", asset->get_text(), false);
		}
		catch (const std::exception& e) {
			sys_print(Warning, "DragDropPreview: failed to parse prefab %s: %s\n", path.c_str(), e.what());
			return;
		}
		PrefabAsset::wire_hierarchy(scratch);

		for (BaseUpdater* bu : scratch.all_obj_vec) {
			Entity* src_entity = bu ? bu->cast_to<Entity>() : nullptr;
			if (!src_entity)
				continue;

			// Only mesh/light-bearing source entities get a ghost -- no decals, no gameplay
			// components are ever spawned or started for the preview.
			bool has_qualifying = false;
			for (Component* c : src_entity->get_components())
				has_qualifying |= is_prefab_ghost_component(c);
			if (!has_qualifying)
				continue;

			const glm::mat4 local = composed_prefab_local_transform(src_entity);

			Entity* g = eng->get_level()->spawn_entity();
			g->dont_serialize_or_edit = true;
			g->set_ws_transform(where * local);

			for (Component* c : src_entity->get_components()) {
				if (!is_prefab_ghost_component(c))
					continue;
				Component* gc = g->create_component(&c->get_type());
				copy_component_fields(gc, c);
			}

			prefab_ghosts.push_back(g);
			prefab_ghost_local_transforms.push_back(local);
		}

		scratch.delete_objs();

		for (auto& g : prefab_ghosts) {
			if (Entity* e = g.get())
				fixup_ghost_entity(e);
		}
	}
	for (size_t i = 0; i < prefab_ghosts.size(); ++i) {
		if (Entity* g = prefab_ghosts[i].get())
			g->set_ws_transform(where * prefab_ghost_local_transforms[i]);
	}
}

void DragDropPreview::tick() {
	if (!had_state_set) {
		delete_obj();
		state = State::None;
		assert(!obj_ptr);
		assert(prefab_ghosts.empty());
	}
	else {
		had_state_set = false;
	}
}
#include "Game/Components/ArrowComponent.h"
#include "Game/Components/BillboardComponent.h"
void DragDropPreview::fixup_ghost_entity(Entity* root) {
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
	if (root)
		set_r(set_r, root);
}

void DragDropPreview::fixup_entity() {
	fixup_ghost_entity(obj_ptr.get());
}

void DragDropPreview::delete_obj() {
	Entity* e = obj_ptr.get();
	if (e) {
		e->destroy_deferred();
	}
	obj_ptr = nullptr;

	for (auto& g : prefab_ghosts) {
		if (Entity* ge = g.get())
			ge->destroy_deferred();
	}
	prefab_ghosts.clear();
	prefab_ghost_local_transforms.clear();
}
