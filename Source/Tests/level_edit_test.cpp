
#include "LevelEditor/EditorDocLocal.h"
#include "Unittest.h"
#include "GameEngineLocal.h"
#include "Assets/AssetDatabase.h"
#include "LevelEditor/Commands.h"
#include "Game/Components/MeshComponent.h"
#include "Framework/MathLib.h"
#include <ctime>
class EditorTestBench
{
public:
	~EditorTestBench() {
		close_editor();
	}
	void start_editor(const char* file, bool for_prefab) {
		if (!for_prefab) {
			Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW,
				string_format("start_ed Map %s", file));
		}
		else {
			Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW,
				string_format("start_ed Prefab %s", file));
		}
		TEST_TRUE(ed_doc.get_is_open());
		TEST_TRUE(eng_local.get_state() == Engine_State::Loading);
		TEST_TRUE(ed_doc.get_doc_name() == file);
		eng_local.state_machine_update();
		TEST_TRUE(eng_local.get_state() == Engine_State::Idle);
		g_assets.finish_all_jobs();
		TEST_TRUE(eng_local.get_state() == Engine_State::Game);
		TEST_TRUE(eng_local.get_level());
		//TEST_TRUE(eng_local.get_level()->get_source_asset()->get_name() == file);
	}
	void close_editor() {
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "close_ed");
		TEST_TRUE(!ed_doc.get_is_open());
		eng_local.state_machine_update();
		TEST_TRUE(eng_local.get_state() == Engine_State::Idle);
	}
	void add_command_and_execute(Command* c) {
		ed_doc.command_mgr->add_command(c);
		errored_command_count += ed_doc.command_mgr->execute_queued_commands();
	}
	void undo() {
		if (errored_command_count == 0)
			ed_doc.command_mgr->undo();
		else {
			sys_print(Debug, "skipping undo %d\n", errored_command_count);
			errored_command_count--;
		}
	}

	int errored_command_count = 0;

	int number = 0;
	Entity* create_entity(Entity* parented = nullptr) {
		add_command_and_execute(new CreateCppClassCommand("Entity", glm::mat4(1.f), EntityPtr(parented), false
		));
		auto& selection = ed_doc.selection_state;
		TEST_TRUE(selection->has_only_one_selected());
		auto ent = selection->get_only_one_selected();
		ent->set_editor_name(std::to_string(number++));
		return selection->get_only_one_selected().get();
	}
	void parent_to(Entity* child, Entity* parent) {
		add_command_and_execute(new ParentToCommand({ child }, parent, false, false));
		TEST_TRUE(child->get_parent() == parent);
	}
	std::vector<EntityPtr> find_random_entities(Random& r, int min, int max) {
		int count = r.RandI(min, max);
		std::vector<EntityPtr> ents;
		for (int i = 0; i < count; i++) {
			ents.push_back(EntityPtr(find_random_entity(r)));
		}
		return ents;
	}
	Entity* find_random_entity(Random& r) {
		auto& objs = eng->get_level()->get_all_objects();
		if (objs.num_used == 0) return nullptr;
		const int max_iters = 100;
		for (int iter = 0; iter < max_iters; iter++) {

			int index = r.RandI(0, objs.items.size() - 1);
			auto handle = objs.items.at(index).handle;
			if (handle == 0 || handle == objs.TOMBSTONE)
				continue;
			if (!objs.items.at(index).data)
				continue;
			auto ent = objs.items.at(index).data->cast_to<Entity>();
			if (ent && !ent->dont_serialize_or_edit)
				return ent;
		}
		return nullptr;
	}
	Entity* find_by_string_name(const std::string& name) {
		auto& objs = eng->get_level()->get_all_objects();
		for (auto o : objs) {
			if (o->is_a<Entity>()) {
				auto e = o->cast_to<Entity>();
				if (e->get_editor_name() == name)
					return e;
			}
		}
		return nullptr;
	}

};

static void do_testing(EditorTestBench& bench, bool for_prefab = false)
{
	Random r(time(NULL));
	sys_print(Info, "state initial: %ul\n", r.state);
	const int ITERATIONS = 1000;

	auto do_iteration = [&](auto&& self, bool force_undo) -> void {

		if (force_undo) {
			sys_print(Info, "BRANCH ITER\n");
		}

		bool do_undo = r.RandF() > 0.7 || force_undo;
		bool do_a_second_undo = r.RandF() > 0.7 && !force_undo;

		bool nest_iteration = r.RandF() > 0.4;
		if (force_undo)
			nest_iteration = r.RandF() > 0.7;
		nest_iteration = nest_iteration && !for_prefab;

		if (do_a_second_undo)
			bench.undo();

		int what = r.RandI(0, 6);
		if (what == 0) {
			const int count = r.RandI(1, 5);
			for (int i = 0; i < 1; i++) {
				bool makeparent = r.RandF() > 0.7;
				Entity* p = nullptr;
				if (makeparent) {
					p = bench.find_random_entity(r);
				}
				auto ent = bench.create_entity(p);
				if (!for_prefab) {
					TEST_TRUE(ent->get_parent() == p);
				}
				else {
					if (!p) {
						TEST_TRUE(ent->get_parent());
					}
					else {
						TEST_TRUE(ent->get_parent() == p);
					}
				}
				auto ptr = ent->get_self_ptr();
				if (do_undo) {

					if (nest_iteration)
						self(self, true);

					bench.undo();
					TEST_TRUE(!ptr);
				}
			}
		}
		else if (what == 1) {
			Entity* p = bench.find_random_entity(r);
			if (p) {
				auto ptr = p->get_self_ptr();
				auto preparent = EntityPtr(ptr->get_parent());
				bench.add_command_and_execute(new RemoveEntitiesCommand({ ptr }));
				TEST_TRUE(!ptr.get() || (for_prefab && !preparent));

				if (do_undo) {

					if (nest_iteration)
						self(self, true);

					bench.undo();
					TEST_TRUE(ptr && ptr->get_parent() == preparent.get());
				}
			}
		}
		else if (what == 2) {
			//auto ents = bench.find_random_entities(r, 2, 10);
			//bench.add_command_and_execute(new RemoveEntitiesCommand(ents));
			//for (auto e : ents) {
			//	TEST_TRUE(!e.get());
			//}
			//if (do_undo) {
			//	bench.undo();
			//}
		}
		else if (what == 3) {
			Entity* p1 = bench.find_random_entity(r);
			Entity* p2 = bench.find_random_entity(r);
			if (p1 && p2 && p1 != p2) {
				EntityPtr p1ptr = p1->get_self_ptr();
				EntityPtr p2ptr = p2->get_self_ptr();

				auto pre_parent = p1->get_parent();
				EntityPtr pre_parentptr(pre_parent);
				bench.add_command_and_execute(new ParentToCommand({ p1 }, p2, false, false));
				TEST_TRUE(p1->get_parent() == p2);
				if (do_undo) {
					if (nest_iteration)
						self(self, true);

					bench.undo();
					TEST_TRUE(p1ptr && p2ptr);
					TEST_TRUE(p1ptr->get_parent() == pre_parentptr.get());
				}
			}
		}
		else if (what == 4) {
			Entity* p1 = bench.find_random_entity(r);
			if (p1) {
				EntityPtr p1ptr = p1->get_self_ptr();
				auto pre_parent = p1->get_parent();
				EntityPtr preparentptr(pre_parent);
				bench.add_command_and_execute(new ParentToCommand({ p1 }, nullptr, true, false));
				TEST_TRUE(p1->get_parent() || (for_prefab && !pre_parent));
				if (do_undo) {

					if (nest_iteration)
						self(self, true);
					bench.undo();
					TEST_TRUE(p1ptr);
					TEST_TRUE(p1ptr->get_parent() == preparentptr.get());
				}
			}
		}
		else if (what == 5) {
			Entity* p1 = bench.find_random_entity(r);
			if (p1 && p1->get_parent()) {
				bench.add_command_and_execute(new ParentToCommand({ p1->get_parent() }, nullptr, false, true));
				if (!for_prefab) {
					TEST_TRUE(!p1->get_parent());
				}
				else {
					TEST_TRUE(p1->get_parent());
				}
				if (do_undo) {
					if (nest_iteration)
						self(self, true);
					bench.undo();
					TEST_TRUE(p1->get_parent());
				}
			}
		}
		else if (what == 6) {
			Entity* p = bench.find_random_entity(r);
			EntityPtr preptr(p);
			if (p)
				bench.add_command_and_execute(new DuplicateEntitiesCommand({ EntityPtr(p) }));
			if (do_undo && p) {
				TEST_TRUE(preptr);
				if (nest_iteration)
					self(self, true);
				bench.undo();
				TEST_TRUE(preptr);
			}
		}
	};

	for (int i = 0; i < ITERATIONS; i++) {
		do_iteration(do_iteration, false);
		bench.errored_command_count = 0;
	}
}
ADD_TEST(le_prefab_testing)
{
	EditorTestBench bench;
	bench.start_editor("", true);
	do_testing(bench, true);
}
ADD_TEST(le_command_testing)
{
	auto& selection = ed_doc.selection_state;

	EditorTestBench bench;
	bench.start_editor("", false);

	{
		bench.add_command_and_execute(new CreateCppClassCommand("Entity", glm::mat4(1.f), EntityPtr(), false
		));
		TEST_TRUE(selection->has_only_one_selected());
		auto ent = selection->get_only_one_selected();
		Entity* e = ent.get();
		TEST_TRUE(ent.get());
		bench.add_command_and_execute(new CreateComponentCommand(ent.get(), &MeshComponent::StaticType));
		TEST_TRUE(ent->get_components().size() == 1 && ent->get_components()[0]->is_a<MeshComponent>());
		bench.undo();
		TEST_TRUE(ent->get_components().empty());
		bench.undo();
		TEST_TRUE(ent.get() == nullptr);

	}
	{
		auto ent1 = bench.create_entity();
		auto ent2 = bench.create_entity(ent1);
		TEST_TRUE(ent2->get_parent() == ent1);
		bench.parent_to(ent1, ent2);
		TEST_TRUE(ent1->get_parent() == ent2);
		bench.undo();
		TEST_TRUE(ent2->get_parent() == ent1);
		bench.undo();
		TEST_TRUE(ent1->get_children().size() == 0);
	}
	do_testing(bench);
}

ADD_TEST(le_basic)
{
	EditorTestBench bench;
	bench.start_editor("test/map0.tmap", false);

}
