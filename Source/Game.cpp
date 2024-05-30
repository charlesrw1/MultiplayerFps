#include "Server.h"
#include "Types.h"
#include "Level.h"
#include "Player.h"
#include "Game_Engine.h"
#include "Net.h"
#include "EntityTypes.h"

#include "DrawPublic.h"


void find_spawn_position(Entity* ent)
{
	int index = eng->find_by_classtype(0, NAME("Spawnpoint") );
	bool found = false;
	while (index != -1) {
		Entity& e = *eng->get_ent(index);

		// do a small test for nearby players
		GeomContact contact = eng->phys.trace_shape(Trace_Shape(e.position, CHAR_HITBOX_RADIUS), ent->selfid, PF_PLAYERS);

		if (!contact.found) {
			ent->position = e.position;
			ent->rotation = e.rotation;
			return;
		}
		
		index = eng->find_by_classtype(index + 1, NAME("Spawnpoint") );
	}

	sys_print("No valid spawn positions for entity\n");

	ent->position = glm::vec3(0);
	ent->rotation = glm::vec3(0);
}

enum Door_State
{
	DOOR_CLOSED,
	DOOR_OPENING,
	DOOR_OPENED
};

void Door::update()
{
	rotation.y = eng->time;
}
void Door::spawn()
{

}

void NPC::spawn()
{

}

void Game_Engine::populate_map()
{
	// call spawn functions
	auto& spawners = level->loadfile.spawners;
	for (int i = 0; i < spawners.size(); i++) {
		if (spawners[i].type != NAME("entity") && spawners[i].type != NAME(""))
			continue;
		const char* classname = spawners[i].dict.get_string("classname");

		Entity* e = create_entity(classname, -1);
		if (!e)
			continue;
		e->spawn();
	}
}

void Game_Engine::client_leave(int slot)
{
	sys_print("client leave\n");
	entityhandle handle = ents[slot]->selfid;
	free_entity(handle);
}

void Entity::initialize_animator(const Animation_Tree_CFG* graph, IAnimationGraphDriver* driver) {

	ASSERT(model);

	if (!animator)
		animator.reset(new Animator());
	animator->initialize_animator(model,graph, driver, this);
}


void Game_Engine::make_client(int slot)
{
	sys_print("making client %d\n", slot);
	Player* p = (Player*)create_entity("Player", slot);
	p->spawn();
}
#include "EntityTypes.h"

Entity* Game_Engine::create_entity(const char* classname, int forceslot)
{
	int slot = forceslot;
	if (slot == -1) {
		for (slot = MAX_CLIENTS; slot < NUM_GAME_ENTS; slot++) {
			if (!ents[slot])
				break;
		}
	}
	ASSERT(slot != NUM_GAME_ENTS && "create_entity overfow");
	
	Entity* e = nullptr;
	e = get_entityfactory().createObject(classname);
	if (!e) {
		printf("no class defined for name %s\n", classname);
		return nullptr;
	}

	num_entities++;
	e->selfid = (entityhandle)slot;
	ents[slot] = e;

	return e;
}

int Game_Engine::find_by_classtype(int index, StringName classtype)
{
	for (int i = index; i < NUM_GAME_ENTS; i++) {
		if (!ents[i]) continue;
		if (ents[i]->get_classname() == classtype)
			return i;
	}
	return -1;
}

void Game_Engine::free_entity(entityhandle handle)
{
	ASSERT(handle < NUM_GAME_ENTS);

	delete ents[handle];
	ents[handle] = nullptr;
	num_entities -= 1;
}

void Game_Engine::fire_bullet(Entity* from, vec3 direction, vec3 origin)
{
	if (!is_host())
		return;

	Ray r(origin, direction);
	RayHit hit = eng->phys.trace_ray(r, from->selfid, PF_ALL);
	if (hit.hit_world) {
		//local.pm.add_dust_hit(hit.pos-direction*0.1f);
		// add particles + decals here

		//local.pm.add_blood_effect(hit.pos, -direction);
	}
	else if(hit.ent_id!=-1){
		Entity* hit_entitiy = eng->get_ent_from_handle(hit.ent_id);
		//hit_entitiy->damage(from, direction, 100);

		//local.pm.add_blood_effect(hit.pos, -direction);
	}

	//create_grenade(from, origin + direction * 0.5f, direction);
}

Player* Game_Engine::get_client_player(int slot)
{
	ASSERT(slot < MAX_CLIENTS);
	if (!ents[slot]) return nullptr;
	ASSERT(ents[slot]->get_classname() == "Player");
	return (Player*)ents[slot];
}


RenderInterpolationComponent::InterpolationData& RenderInterpolationComponent::get(int index)
{
	int s = interpdata.size();
	index = ((index % s) + s) % s;
	return interpdata[index];
}

static float mid_lerp(float min, float max, float mid_val)
{
	return (mid_val - min) / (max - min);
}
float modulo_lerp(float start, float end, float mod, float alpha)
{
	float d1 = glm::abs(end - start);
	float d2 = mod - d1;


	if (d1 <= d2)
		return glm::mix(start, end, alpha);
	else {
		if (start >= end)
			return fmod(start + (alpha * d2), mod);
		else
			return fmod(end + ((1 - alpha) * d2), mod);
	}
}

void RenderInterpolationComponent::evaluate(float time)
{
	int entry_index = 0;
	for (; entry_index < interpdata.size(); entry_index++) {
		auto& e = get(interpdata_head - 1 - entry_index);
		if (time >= e.time)
			break;
	}

	if (entry_index == interpdata.size()) {
		printf("all entries greater than time\n");
		ASSERT(get(interpdata_head - 1).time >= 0.f);
		lerped_pos = get(interpdata_head - 1).position;
		lerped_rot = get(interpdata_head - 1).rotation;
		return;
	}
	if (entry_index == 0) {
		printf("all entries less than time\n");
		float diff = time - get(interpdata_head - 1).time;
		ASSERT(diff >= 0.f);
		if (diff > max_extrapolate_time) {
			lerped_pos = get(interpdata_head - 1).position;
			lerped_rot = get(interpdata_head - 1).rotation;
		}
		else {
			printf("extrapolate\n");
			auto& a = get(interpdata_head - 2);
			auto& b = get(interpdata_head - 1);
			float extrap = mid_lerp(a.time, b.time, time);
			lerped_pos = glm::mix(a.position, b.position, extrap);
			for (int i = 0; i < 3; i++)
				lerped_rot[i] = modulo_lerp(a.rotation[i], b.rotation[i], TWOPI, extrap);
		}

		return;
	}

	auto& a = get(interpdata_head - 1 - entry_index);
	auto& b = get(interpdata_head - entry_index);

	if (a.time < 0 && b.time < 0) {
		printf("no state\n");
		lerped_pos = glm::vec3(0.f);
		lerped_rot = glm::vec3(0.f);
	}
	else if (a.time < 0) {
		printf("only one state\n");
		lerped_pos = b.position;
		lerped_rot = b.rotation;
	}
	else if (b.time < 0) {
		ASSERT(0);
	}
	else {
		ASSERT(a.time < b.time);
		ASSERT(a.time <= time && b.time >= time);

		float dist = glm::length(b.position - a.position);
		float speed = dist / (b.time - a.time);

		if (speed > telport_velocity_threshold) {
			lerped_pos = b.position;
			lerped_rot = b.rotation;
			sys_print("teleport\n");
		}
		else {
			float midlerp = mid_lerp(a.time, b.time, time);
			lerped_pos = glm::mix(a.position, b.position, midlerp);
			for (int i = 0; i < 3; i++)
				lerped_rot[i] = modulo_lerp(a.rotation[i], b.rotation[i], TWOPI, midlerp);

		}
	}

}

void RenderInterpolationComponent::add_state(float time, vec3 p, vec3 r)
{
	auto& element = get(interpdata_head);
	element.time = time;
	element.position = p;
	element.rotation = r;
	interpdata_head = (interpdata_head + 1) % interpdata.size();
}

void RenderInterpolationComponent::clear()
{
	for (int i = 0; i < interpdata.size(); i++) interpdata[i].time = -1.f;
}


void Entity::projectile_physics()
{
	for (int i = 0; i < 5; i++) {

	}
}
void Entity::gravity_physics()
{
	bool bounce = true;
	bool slide = true;

	velocity.y -= (12.f * eng->tick_interval);
	int bumps = 4;
	bool hit_a_wall = false;
	float time = eng->tick_interval;
	vec3 pre_velocity = velocity;
	glm::vec3 first_n;
	GeomContact contact{};
	for (int i = 0; i < bumps; i++) {
		vec3 end = position + velocity * (time / bumps);
		
		contact = eng->phys.trace_shape(Trace_Shape(end, phys_opt.get_radius()), selfid, PF_WORLD);

		if (contact.found) {
			position = end + contact.penetration_normal * (contact.penetration_depth);

			if (!hit_a_wall) {
				hit_a_wall = true;
			}
			first_n = contact.surf_normal;

			if (1) {
				vec3 penetration_velocity = dot(velocity, contact.penetration_normal) * contact.penetration_normal;
				vec3 slide_velocity = velocity - penetration_velocity;
				velocity = slide_velocity;
			}
		}
		else position = end;
	}
	if (hit_a_wall) {
		collide(nullptr, contact);
		if (bounce) {
			velocity = glm::reflect(pre_velocity, first_n);
			velocity *= 0.6f;
		}
	}
}
#include <PxPhysics.h>

void Entity::update()
{
	gravity_physics();
}



#include "Framework/MeshBuilder.h"
#include "Framework/Config.h"


static Random fxrand = Random(1452345);

void grenade_hit_wall(Entity* ent, glm::vec3 normal)
{
	if (glm::length(ent->velocity)>1.f) {
		ent->rotation = vec3(fxrand.RandF(0, TWOPI), fxrand.RandF(0, TWOPI), fxrand.RandF(0, TWOPI));
		//eng->local.pm.add_dust_hit(ent->position + normal * 0.1f);
	}
}



Grenade::Grenade()
{
}

void Grenade::spawn()
{
	set_model("grenade_he.glb");
	phys_opt.type = Ent_PhysicsType::Complex;
	//phys_opt.size.x = grenade_radius.real();
	phys_opt.shape = Ent_PhysicsShape::Sphere;
	throw_time = eng->time;
}


void Grenade::update()
{
	Entity::update();

	if (eng->time - throw_time > 2.5) {
		sys_print("BOOM\n");
		eng->free_entity(selfid);
	}
}

Entity* create_grenade(Entity* thrower, glm::vec3 org, glm::vec3 direction)
{
	ASSERT(thrower);
	Grenade* e = (Grenade*)eng->create_entity("Grenade");
	e->thrower = thrower->selfid;
	e->position = org;
	//e->velocity = direction * grenade_vel.real();
	return e;
}
Entity::~Entity()
{
	printf("removed handle %d\n",render_handle.id);
	idraw->remove_obj(render_handle);

}
Entity::Entity()
{

}

void Entity::spawn()
{

}

void Entity::present()
{
	if (!render_handle.is_valid() && model)
		render_handle = idraw->register_obj();
	else if (render_handle.is_valid() && !model)
		idraw->remove_obj(render_handle);

	if (render_handle.is_valid() && model) {
		Render_Object proxy;

		proxy.visible = !(state_flags & EF_HIDDEN);
		if (animator)
			proxy.animator = animator.get();
		proxy.model = model;

		// FIXME:
		proxy.transform = get_world_transform();// * model->skeleton_root_transform;
		
		idraw->update_obj(render_handle, proxy);
	}
}