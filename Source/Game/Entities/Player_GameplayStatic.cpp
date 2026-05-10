// GameplayStatic method implementations

#include "Player.h"
#include "Framework/Util.h"
#include "Framework/Config.h"

#include "GameEnginePublic.h"

#include "Debug.h"

#include "Assets/AssetDatabase.h"

#include "Level.h"

#include "Physics/Physics2.h"
#include "Physics/ChannelsAndPresets.h"

#include "UI/GUISystemPublic.h"

#include "Game/GameplayStatic.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/PhysicsComponents.h"

#include "UI/UILoader.h"

#include "Game/Components/SpawnerComponenth.h"

#include <cassert>

using std::string;
using std::vector;

vector<Component*> GameplayStatic::find_components(const ClassTypeInfo* info) {
	ASSERT(info);
	ASSERT(info->is_a(Component::StaticType));
	double now = GetTime();
	auto& all = eng->get_level()->get_all_objects();
	vector<Component*> out;
	for (auto e : all)
		if (e->get_type().is_a(*info))
			out.push_back((Component*)e);
	double end = GetTime();
	printf("find_components_of_class: took %f\n", float(end - now));
	return out;
}

Entity* GameplayStatic::find_by_name(string name) {
	ASSERT(!name.empty());
	return eng->get_level()->find_initial_entity_by_name(name);
}

static int GameplayStatic_debug_text_start = 10;

void GameplayStatic::reset_debug_text_height() {
	GameplayStatic_debug_text_start = 10;
}

void GameplayStatic::debug_text(string text) {
	ASSERT(!text.empty());
	auto font = g_assets.find_sync<GuiFont>("eng/fonts/monospace12.fnt").get();
	auto draw_text = [&](const char* s) {
		string str = s;
		TextShape shape;
		Rect2d size = GuiHelpers::calc_text_size(std::string_view(str), font);
		glm::ivec2 ofs = GuiHelpers::calc_layout({-100, -10}, guiAnchor::Center, UiSystem::inst->get_vp_rect());

		shape.rect.x = ofs.x;
		shape.rect.y = ofs.y + size.h + GameplayStatic_debug_text_start;
		shape.font = font;
		shape.color = COLOR_WHITE;
		shape.with_drop_shadow = true;
		shape.drop_shadow_ofs = 1;
		shape.text = str;
		UiSystem::inst->window.draw(shape);
		GameplayStatic_debug_text_start += size.h;
	};
	draw_text(text.c_str());
}

void GameplayStatic::debug_line_normal(glm::vec3 p, glm::vec3 n, float len, float life, const lColor& color) {
	ASSERT(len >= 0.0f);
	Debug::add_line(p, p + n * len, color.to_color32(), life);
}

int GameplayStatic::get_collision_mask_for_physics_layer(PL physics_layer) {
	return (int)::get_collision_mask_for_physics_layer(physics_layer);
}

Entity* GameplayStatic::spawn_entity() {
	ASSERT(eng->get_level());
	return eng->get_level()->spawn_entity();
}

std::vector<SpawnerComponent*> GameplayStatic::find_spawners_in_class(std::string name) {
	ASSERT(!name.empty());
	std::vector<SpawnerComponent*> test_ents;
	for (auto e : eng->get_level()->get_all_objects()) {
		if (auto s = e->cast_to<SpawnerComponent>()) {
			if (s->get_spawner_type() == name)
				test_ents.push_back(s);
		}
	}
	return test_ents;
}

HitResult GameplayStatic::cast_ray(glm::vec3 start, glm::vec3 end, int channel_mask, PhysicsBody* ignore_this) {
	ASSERT(channel_mask != 0);
	HitResult out;
	world_query_result res;
	TraceIgnoreVec ignore;
	if (ignore_this)
		ignore.push_back(ignore_this);

	g_physics.trace_ray(res, start, end, &ignore, channel_mask);
	out.hit = res.component != nullptr;
	if (res.component) {
		out.pos = res.hit_pos;
		out.what = res.component->get_owner();
		out.normal = res.hit_normal;
	}
	return out;
}

std::vector<obj<Entity>> GameplayStatic::sphere_overlap(glm::vec3 center, float radius, int channel_mask) {
	ASSERT(radius > 0.0f);
	std::vector<obj<Entity>> outVec;
	overlap_query_result res;
	g_physics.sphere_is_overlapped(res, radius, center, channel_mask);
	for (int i = 0; i < (int)res.overlaps.size(); i++) {
		outVec.push_back(res.overlaps[i]->get_owner());
	}
	return outVec;
}

void GameplayStatic::debug_sphere(glm::vec3 center, float r, float life, const lColor& color) {
	ASSERT(r >= 0.0f);
	Debug::add_sphere(center, r, color.to_color32(), life);
}

extern string print_vector(glm::vec3 v);

void GameplayStatic::enable_ragdoll_shared(Entity* e, bool enable) {
	ASSERT(e);
	MeshComponent* mesh = e->get_cached_mesh_component();
	ASSERT(mesh);
	AnimatorObject* animator = mesh->get_animator();
	ASSERT(animator);
	// animator->set_update_owner_position_to_root(true);
	auto& children = e->get_children();
	for (auto c : children) {

		auto phys = c->get_component<PhysicsBody>();
		if (!phys || phys->is_a<AdvancedJointComponent>())
			continue;

		auto m = e->get_cached_mesh_component();
		if (!m || !m->get_animator())
			continue;
		int i = m->get_index_of_bone(c->get_parent_bone());
		if (i == -1)
			continue;

		const glm::mat4& this_ws = e->get_ws_transform();

		if (enable) {
			auto cur = c->get_ls_transform();
			auto theFinalMat = this_ws * m->get_animator()->get_global_bonemats().at(i);
			string msg = std::string(c->get_parent_bone().get_c_str()) + ": " + print_vector(theFinalMat[1]);
			msg += " --- " + print_vector(cur[3]);

			eng->log_to_fullscreen_gui(Info, msg.c_str());
			sys_print(Info, msg.c_str());

			phys->enable_with_initial_transforms(this_ws * m->get_animator()->get_last_global_bonemats().at(i) * cur,
												 this_ws * m->get_animator()->get_global_bonemats().at(i) * cur,
												 eng->get_dt());
		} else {
			phys->set_is_enable(false);
		}
	}
}
