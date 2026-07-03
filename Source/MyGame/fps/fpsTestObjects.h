#pragma once

#include <MyGame/fps/fpsObjects.h>
#include <Render/Model.h>
#include <Framework/BoolButton.h>
#include <Game/Components/MeshComponent.h>
#include <Debug.h>
static const std::string_view FPSDEFAULTBILLBOARD = "eng/editor/script_cxx.png";


class PhysicsLayerTesterTriggerScript : public Component {
public:
	CLASS_BODY(PhysicsLayerTesterTriggerScript);
	PhysicsLayerTesterTriggerScript() { set_call_init_in_editor(true);
	}

	void on_collider_trigger(Entity* other, bool entered) override { 
		ASSERT(other);
		if (entered) {
			current_entities.insert(other->get_instance_id());
		} else {
			current_entities.erase(other->get_instance_id());
		}
		if (current_entities.size() >= 1) {
			set_ticking(true);
		}
	}
	void update() override {
		if (current_entities.size() == 0) {
			set_ticking(false);
			return;
		}
		std::vector<int64_t> to_remove;
		for (auto i : current_entities) {
			Entity* e = EntityPtr(i).get();
			if (!e)
				to_remove.push_back(i);
			else {
				Debug::add_line(get_ws_position(), e->get_ws_position(), COLOR_GREEN, 0.f);
			}
		}
		for (auto i : to_remove)
			current_entities.erase(i);
		Debug::add_text(get_ws_position(), string_format("Num entities in trigger: %d", (int)current_entities.size()),
						COLOR_WHITE, 0.f);
	}

	std::unordered_set<int64_t> current_entities;
};

class PhysicsLayerTesterObject : public Component {
public:
	CLASS_BODY(PhysicsLayerTesterObject, spawnable);

	PhysicsLayerTesterObject() {
		set_call_init_in_editor(true);
	}
	void editor_start() override {
		editor_set_billboard(FPSDEFAULTBILLBOARD);
	}
	void update_subobject() {
		auto* myent = subobject.get();
		if (myent)
			myent->destroy();
		myent = GameplayStatic::spawn_entity();

		myent->parent_to(nullptr);
		myent->dont_serialize_or_edit = true;

		PhysicsBody* physics = nullptr;
		if (collider_type == 0) {
			// mesh
			auto* mymesh = myent->get_component<MeshComponent>();
			if (!mymesh)
				mymesh = myent->create_component<MeshComponent>();
			mymesh->set_model(model.get());
			physics = myent->get_component<PhysicsBody>();
		} else {
			if (collider_type == 1) {
				physics = myent->create_component<SphereComponent>();
			}
			else {
				physics = myent->create_component<BoxComponent>();
			}
		}
		if (!physics)
			physics = myent->create_component<BoxComponent>();


		physics->set_physics_layer(PL(layer));
		
		if (bodytype==BodyType::Dynamic) {
			if (simulating)
				physics->set_body_type(bodytype);
			else
				physics->set_body_type(BodyType::Kinematic);
		} else {
			physics->set_body_type(bodytype);		
		}
		physics->set_is_trigger(is_trigger);
		if (is_trigger) {
			auto* testerscript = myent->get_component<PhysicsLayerTesterTriggerScript>();
			if (!testerscript)
				testerscript = myent->create_component<PhysicsLayerTesterTriggerScript>();
		}

		if (simulating) {
			physics->teleport_to(get_owner()->get_ws_transform());
		} else {
			myent->parent_to(get_owner());
			myent->set_ls_transform(glm::mat4(1.f));
		}


		subobject = myent;
	}

	void start() override { 
		update_subobject();
	}
	void stop() override  {

	}
	void update() { set_ticking(false);
	}
	void editor_on_change_property() override {
		if (simulate_toggle.check_and_swap()) {
			simulating = !simulating;
		}
		if (respawn.check_and_swap()) {

		}
		update_subobject();
	}

	EntityPtr subobject;

	REF int collider_type = 0;
	REF AssetPtr<Model> model;
	REF AssetPtr<MaterialInstance> trigger;

	REF BodyType bodytype = BodyType::Static;
	REF bool is_trigger = false;

	bool simulating = false;

	REF BoolButton simulate_toggle;
	REF BoolButton respawn;
	
	REF int layer = 0;
};
class PhysicsLayerTesterRaycast : public Component
{
public:
	CLASS_BODY(PhysicsLayerTesterRaycast,spawnable);

	PhysicsLayerTesterRaycast() {
	}
	void editor_start() override { editor_set_billboard(FPSDEFAULTBILLBOARD); }
	REF bool layer0 = false;
	REF bool layer1 = false;
	REF bool layer2 = false;
	REF bool layer3 = false;
	REF bool layer4 = false;
	REF bool layer5 = false;
};