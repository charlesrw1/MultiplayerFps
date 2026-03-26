#include "EngineTesterApplication.h"

LuaMiscFuncs* LuaMiscFuncs::inst = nullptr;

// test lua integration and general object stuff
class ET_GenericTest : public EngineTestcase
{
public:
	ET_GenericTest() { name = "generic_test"; }
	bool update(bool focused) final { return false; }
	void integration_tick(IntegrationTester& t) final {
		Level* l = eng->get_level();
		auto app = eng->get_app()->cast_to<EngineTesterApp>();

		ASSERT(app);

		// test if lua works with deleted objects
		EntityPtr e = app->make_object_test();
		ASSERT(app->make_object_test_return_exists());
		e->destroy();
		ASSERT(!e.get());
		ASSERT(!app->make_object_test_return_exists());
		e = app->make_object_test();
		ASSERT(app->make_object_test_return_exists());
		e->destroy_deferred();
		ASSERT(e.get());
		ASSERT(app->make_object_test_return_exists());
		t.wait_ticks(1);
		ASSERT(!e.get());
		ASSERT(!app->make_object_test_return_exists());

		ASSERT(app->count_in_vector({}) == 0);
		ASSERT(app->count_in_vector({1, 2, 3}) == 3);
	}
};
// test anims, ragdolls, ik, parenting to bones,
class ET_Anims : public EngineTestcase
{
public:
	const glm::vec3 all_anim_location = glm::vec3(0, 5, 0);
	const int all_anim_width = 8;
	ET_Anims() {
		name = "anims";
		// spawn animated models for testing
		auto l = eng->get_level();
		auto model = Model::load("SWAT_Model.cmdl");
		auto& allClips = model->get_skel()->get_all_clips();
		int index = -1;
		for (auto& [name, clip] : allClips) {
			index += 1;

			glm::vec3 position = all_anim_location + glm::vec3(index / all_anim_width, 0, index % all_anim_width) * 3.f;

			auto mesh = l->spawn_entity()->create_component<MeshComponent>();
			mesh->set_model(model);
			mesh->get_owner()->set_ws_position(position);

			agBuilder builder;
			auto clip = builder.alloc<agClipNode>();
			clip->set_clip(model, name);
			clip->set_looping(true);
			builder.set_root(clip);
			mesh->create_animator(&builder);
		}

		// now do more targeted blend tree tests

		std::vector<SpawnerComponent*> players = EngineTesterApp::find_all_in_class("player_spawn");
		index = 0;
		std::array anims = {"stand_rifle_run_n", "run_forward_unequip", "falling"};
		for (auto player : players) {
			auto mesh = player->get_owner()->create_component<MeshComponent>();
			mesh->set_model(model);
			agBuilder builder;
			auto clip = builder.alloc<agClipNode>();
			clip->set_clip(model, anims[index]);
			clip->set_looping(true);
			builder.set_root(clip);
			mesh->create_animator(&builder);

			index = (index + 1) % (int)anims.size();
		}
		// additive test, ik, blend, slot, statemachine, masked, sync, retarget, modify
	}

	bool update(bool f) {}
};
// anim into ragdoll
class ET_Ragdoll : public EngineTestcase
{
public:
	const std::array<Color32, 2> colors = {COLOR_RED, COLOR_BLUE};

	ET_Ragdoll() {
		name = "ragdoll";

		// ragdolls
		Random rnd(113);
		auto rds = EngineTesterApp::find_all_in_class("ragdoll_test");

		int i = 0;
		for (auto& rd : rds) {
			i++;
			Ragdoll r;
			r.dynamic_mat = imaterials->create_dynmaic_material(MaterialInstance::load("defaultPBR.mm"));
			r.dynamic_mat->set_u8vec_parameter("colorMult", colors.at(i % 2));
			r.transform = rd->get_ws_transform();
			r.lifespan_left = rnd.RandF(0, 1);
			ragdolls.push_back(std::move(r));
		}
	}
	bool update(bool focused) final {
		int i = 0;
		for (auto& r : ragdolls) {
			i++;
			r.lifespan_left -= eng->get_dt();
			if (r.lifespan_left < 0) {
				if (r.ptr.get())
					r.ptr->destroy();
				r.ptr = LuaMiscFuncs::inst->create_ragdoll();
				r.ptr->set_ws_transform(r.transform);
				auto rc = r.ptr->get_component<RagdollComponent>();
				rc->enable();
				r.lifespan_left = 3.7;

				auto mesh = r.ptr->get_component<MeshComponent>();
				mesh->set_material_override(r.dynamic_mat.get());
			}
			float alpha = r.lifespan_left / 3.7;
			glm::vec4 v = color32_to_vec4(colors.at(i % 2));
			v *= alpha;
			r.dynamic_mat->set_u8vec_parameter("colorMult", vec4_to_color32(v));
		}
		return false;
	}
	void integration_tick(IntegrationTester& t) final {}
	struct Ragdoll
	{
		EntityPtr ptr;
		DynamicMatUniquePtr dynamic_mat;
		glm::mat4 transform;
		float lifespan_left = 1.0;
	};
	std::vector<Ragdoll> ragdolls;
};

// physics layer tests. on overlap, raycast, on hit, materials. joints.
// ragdolls. features physics gun.
class ET_PhysicsTest : public EngineTestcase
{
public:
	ET_PhysicsTest() { eng->load_level("physics_test_world.tmap"); }
};
class ET_ShadowmapAtlas : public EngineTestcase
{
public:
};

using std::make_unique;

static std::vector<uptr<EngineTestcase>> make_main_test() {
	vector<uptr<EngineTestcase>> output;
	auto add = [&](auto ptr) { output.push_back(uptr<EngineTestcase>(ptr)); };
	add(new ET_GenericTest);
	add(new ET_Ragdoll);
	add(new ET_Anims);
	add(new ET_PhysicsTest);
	add(new ET_ShadowmapAtlas);

	return output;
}

EngineTesterApp::EngineTesterApp() {
	if (!LuaMiscFuncs::inst) {
		LuaMiscFuncs::inst = ClassBase::create_class<LuaMiscFuncs>("LuaMiscFuncsImpl");
		ASSERT(LuaMiscFuncs::inst);
	}
}

std::vector<SpawnerComponent*> EngineTesterApp::find_all_with_name_ordered(string name, string class_) {
	auto all = find_all_in_class(class_);
	std::vector<SpawnerComponent*> matches;
	for (auto s : all) {
		if (s->get_owner()->get_editor_name().find(name) != std::string::npos)
			matches.push_back(s);
	}
	std::sort(matches.begin(), matches.end(), [](SpawnerComponent* a, SpawnerComponent* b) {
		return a->get_owner()->get_editor_name() < b->get_owner()->get_editor_name();
	});
	return matches;
}

std::vector<SpawnerComponent*> EngineTesterApp::find_all_in_class(string name) {
	std::vector<SpawnerComponent*> test_ents;
	for (auto e : eng->get_level()->get_all_objects()) {
		if (auto s = e->cast_to<SpawnerComponent>()) {
			if (s->get_spawner_type() == name)
				test_ents.push_back(s);
		}
	}
	return test_ents;
}

void EngineTesterApp::start() {
	lua_start();
	sys_print(Info, "EngineTesterApp:start");

	extern ConfigVar ui_disable_screen_log;
	extern ConfigVar g_drawconsole;
	g_drawconsole.set_bool(false);
	ui_disable_screen_log.set_bool(true);
	SDL_SetWindowSize(eng->get_os_window(), TEST_WINDOW_SIZE.x, TEST_WINDOW_SIZE.y);

	eng->load_level("demo_level_1.tmap");
	Level* l = eng->get_level();
	auto ent = l->spawn_entity();
	auto cam = ent->create_component<CameraComponent>();
	cam->set_is_enabled(true);

	{
		// spawn animated models for testing
		std::vector<SpawnerComponent*> players = find_all_in_class("player_spawn");
		int index = 0;
		std::array anims = {"stand_rifle_run_n", "run_forward_unequip", "falling"};
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
	{
		auto physics = find_all_in_class("prop_physics");
		for (auto p : physics) {
			auto obj = l->spawn_entity();
			obj->set_ws_transform(p->get_ws_transform());
			auto model = Model::load(p->obj["model"]);
			auto mc = obj->create_component<MeshComponent>();
			mc->set_model(model);
			auto mcc = obj->create_component<MeshColliderComponent>();
			ASSERT(mcc);
			mcc->set_is_enable(false);
			mcc->set_is_enable(true);
			mcc->set_is_simulating(true);
			mcc->set_is_static(false);
		}
	}

	auto func = [&](IntegrationTester& tester) {
		std::vector<SpawnerComponent*> test_ents = find_all_in_class("engine_test_case");

		auto ents = find_all_with_name_ordered("path", "engine_test_case");
		CameraPathFollower path(ents);

		int index = 0;
		for (;;) {
			// auto s = test_ents.at(index);
			auto scene_cam = CameraComponent::get_scene_camera();
			scene_cam->set_fov(80);
			// scene_cam->get_owner()->set_ws_transform(s->get_ws_transform());

			// tester.wait_time(2.f);

			// index = (index + 1) % (int)test_ents.size();

			path.update();
			tester.wait_ticks(1);
		}
	};

	std::vector<IntTestCase> cases;
	cases.push_back({func, "main_test", 60.f});
	tester = std::make_unique<IntegrationTester>(false, cases);
}

void EngineTesterApp::update() {
	if (tester)
		tester->tick(eng->get_dt());
}

CameraPathFollower::CameraPathFollower(std::vector<SpawnerComponent*> components) {
	for (auto c : components) {
		points.push_back({c->get_ws_position(), c->get_owner()->get_ws_rotation()});
	}
	time_start = GetTime();
}

glm::vec3 CameraPathFollower::catmull_rom(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t) {
	return 0.5f * ((2.f * p1) + (-p0 + p2) * t + (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t * t +
				   (-p0 + 3.f * p1 - 3.f * p2 + p3) * t * t * t);
}

void CameraPathFollower::update() {
	double time = GetTime() - time_start;
	int index = time / time_per_point;
	index = (index) % points.size();
	int next = (index + 1) % points.size();
	int next2 = (index + 2) % points.size();
	int next3 = (index + 3) % points.size();

	float frac = fmod(time, time_per_point) / time_per_point;
	glm::quat rot = glm::slerp(points.at(index).q, points.at(next).q, frac);
	glm::vec3 pos = glm::mix(points.at(index).p, points.at(next).p, frac);

	CameraComponent::get_scene_camera()->get_owner()->set_ws_transform(pos, rot, glm::vec3(1));
}
