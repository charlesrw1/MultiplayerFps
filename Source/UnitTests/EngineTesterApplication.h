#pragma once
#include <SDL2/SDL.h>
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/CameraComponent.h"
#include "Framework/Config.h"
#include "Game/Components/MeshComponent.h"
#include "Animation/Runtime/Animation.h"
#include "IntegrationTest.h"
#include "Game/Components/SpawnerComponenth.h"
#include "Animation/Runtime/RuntimeNodesNew2.h"
#include "Game/Components/RagdollComponent.h"
class LuaMiscFuncs : public ClassBase {
public:
	CLASS_BODY(LuaMiscFuncs, scriptable);
	static LuaMiscFuncs* inst;
	REF virtual Entity* create_ragdoll() {
		return nullptr;
	}
};


class EngineTesterApp : public Application {
public:
	CLASS_BODY(EngineTesterApp);

	const glm::ivec2 TEST_WINDOW_SIZE = glm::ivec2{ 800,600 };

	EngineTesterApp() {
		if (!LuaMiscFuncs::inst) {
			LuaMiscFuncs::inst = ClassBase::create_class<LuaMiscFuncs>("LuaMiscFuncsImpl");
			ASSERT(LuaMiscFuncs::inst);
		}
	}

	std::vector<SpawnerComponent*> find_all_in_class(string name) {
		std::vector<SpawnerComponent*> test_ents;
		for (auto e : eng->get_level()->get_all_objects()) {
			if (auto s = e->cast_to<SpawnerComponent>()) {
				if (s->get_spawner_type() == name)
					test_ents.push_back(s);
			}
		}
		return test_ents;
	}

	void start() {
		extern ConfigVar ui_disable_screen_log;
		extern ConfigVar g_drawconsole;
		g_drawconsole.set_bool(false);
		ui_disable_screen_log.set_bool(true);
		SDL_SetWindowSize(eng->get_os_window(), TEST_WINDOW_SIZE.x, TEST_WINDOW_SIZE.y);
		
		eng->load_level("cs_map.tmap");
		Level* l = eng->get_level();
		auto ent = l->spawn_entity();
		auto cam = ent->create_component<CameraComponent>();
		cam->set_is_enabled(true);

		{
			// spawn animated models for testing
			std::vector<SpawnerComponent*> players = find_all_in_class("player_spawn");
			int index = 0;
			std::array anims = { "stand_rifle_run_n","run_forward_unequip","falling"};
			for (auto player : players) {
				auto mesh = player->get_owner()->create_component<MeshComponent>();
				auto model = Model::load("SWAT_Model.cmdl");
				mesh->set_model(model);
				agBuilder builder;
				auto clip = builder.alloc<agClipNode>();
				clip->set_clip(model, anims[index]);
				clip->set_looping(true);
				builder.set_root(clip);
				mesh->create_animator(&builder);

				index = (index + 1) % (int)anims.size();
			}
		}
		// ragdolls
		{
			Random rnd(113);
			auto rds = find_all_in_class("ragdoll_test");
			for (auto& rd : rds) {
				Ragdoll r;
				r.transform = rd->get_ws_transform();
				r.lifespan_left = rnd.RandF(0,1);
				ragdolls.push_back(r);
			}
		}

		auto func = [&](IntegrationTester& tester) {
			std::vector<SpawnerComponent*> test_ents = find_all_in_class("engine_test_case");

			int index = 0;
			for (;;) {
				auto s = test_ents.at(index);
				auto scene_cam = CameraComponent::get_scene_camera();
				scene_cam->get_owner()->set_ws_transform(s->get_ws_transform());

				tester.wait_time(2.f);

				index = (index + 1) % (int)test_ents.size();
			}

		};

		std::vector<IntTestCase> cases;
		cases.push_back({ func,"main_test",60.f });
		tester = std::make_unique<IntegrationTester>(false,cases);
	}
	void update() {
		tester->tick(eng->get_dt());

		for (auto& r : ragdolls) {
			r.lifespan_left -= eng->get_dt();
			if (r.lifespan_left < 0) {
				if(r.ptr.get())
					r.ptr->destroy();
				r.ptr = LuaMiscFuncs::inst->create_ragdoll();
				r.ptr->set_ws_transform(r.transform);
				auto rc = r.ptr->get_component<RagdollComponent>();
				rc->enable();
				r.lifespan_left = 3.7;
			}
		}

	}

	struct Ragdoll {
		EntityPtr ptr;
		glm::mat4 transform;
		float lifespan_left = 1.0;
	};

	std::vector<Ragdoll> ragdolls;

	uptr<IntegrationTester> tester;
};