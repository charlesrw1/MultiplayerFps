#include "Server.h"
#include "Types.h"
#include "Level.h"
#include "Player.h"
#include "Game_Engine.h"
#include "Net.h"

void player_update(Entity* ent);
void player_spawn(Entity* ent);

void dummy_update(Entity* ent);

Entity* create_grenade(Entity* from, glm::vec3 org, glm::vec3 vel);
void grenade_update(Entity* ent);


void EntTakeDamage(Entity* ent, Entity* from, int amt);


void find_spawn_position(Entity* ent)
{
	int index = eng->find_by_classtype(0, entityclass::SPAWNPOINT);
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
		
		index = eng->find_by_classtype(index + 1, entityclass::SPAWNPOINT);
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

void spawn_door(Level::Entity_Spawn& spawn)
{
	Door* door = (Door*)eng->create_entity(entityclass::DOOR);

	door->position = spawn.position;
	door->rotation = spawn.rotation;
	door->physics = EPHYS_MOVER;
	
	door->state = Door::CLOSED;

	door->flags = 0;
	door->flags |= EF_SOLID;
}
void spawn_spawn_point(Level::Entity_Spawn& spawn)
{
	Entity* sp = eng->create_entity(entityclass::SPAWNPOINT);
	sp->position = spawn.position;
	sp->rotation = spawn.rotation;
	sp->physics = EPHYS_NONE;

}

void spawn_zone(Level::Entity_Spawn& spawn)
{
	entityclass type = entityclass::SPAWNZONE;
	if (spawn.classname == "bomb_area")
		type = entityclass::BOMBZONE;
	Entity* area = eng->create_entity(type);
	area->position = spawn.position;
	area->rotation = spawn.rotation;
	area->col_size = spawn.scale;
}

struct Spawn_Link
{
	const char* name;
	void(*spawn)(Level::Entity_Spawn& spawn);
};

Spawn_Link links[]
{
	{"door", spawn_door},
	{"player_spawn", spawn_spawn_point},
	{"bomb_area", spawn_zone},
	{"spawn_area", spawn_zone},
	{"cubemap", nullptr },
	{"cubemap_box",nullptr}
};


void Game_Engine::on_game_start()
{
	// call spawn functions
	for (int i = 0; i < level->espawns.size(); i++)
	{
		Level::Entity_Spawn& es = level->espawns.at(i);
		int n = sizeof(links) / sizeof(Spawn_Link);
		int j = 0;
		for (; j < n; j++) {
			if (es.classname == links[j].name) {
				if(links[j].spawn)
					links[j].spawn(es);
				break;
			}
		}
		if (j == n) {
			sys_print("Couldn't find spawn function for Entity_Spawn %s", es.classname.c_str());
		}
	}
}

void Game_Engine::client_leave(int slot)
{
	sys_print("client leave\n");
	entityhandle handle = ents[slot]->selfid;
	free_entity(handle);
}

void Game_Engine::make_client(int slot)
{
	sys_print("making client %d\n", slot);
	Player* p = (Player*)create_entity(entityclass::PLAYER, slot);

	// FIXME: put this elsewhere
	if (eng->is_host) {
		p->interp = unique_ptr<RenderInterpolationComponent>(new RenderInterpolationComponent(3));
	}

	p->init();
	player_spawn(p);
}
#include "EntityTypes.h"

Entity* Game_Engine::create_entity(entityclass classtype, int forceslot)
{
	int slot = forceslot;
	if (slot == -1) {
		for (slot = MAX_CLIENTS; slot < NUM_GAME_ENTS; slot++) {
			if (!ents[slot])
				break;
		}
	}
	ASSERT(slot != NUM_GAME_ENTS && "create_entity overfow");
	
	num_entities++;
	Entity* e = nullptr;
	switch (classtype)
	{
	case entityclass::PLAYER:
		e = new Player;
		break;

	case entityclass::DOOR:
		e = new Door;
		break;
	case entityclass::NPC:
		e = new NPC;
		break;

	case entityclass::BOMBZONE:
	case entityclass::SPAWNZONE:
	case entityclass::SPAWNPOINT:
	case entityclass::EMPTY:
		e = new Entity;
		break;

	case entityclass::THROWABLE:
		e = new Grenade;
		break;

	default:
		ASSERT(!"no entity type");
	}

	e->class_ = classtype;
	e->selfid = (entityhandle)slot;

	ents[slot] = e;

	return e;
}

int Game_Engine::find_by_classtype(int index, entityclass classtype)
{
	for (int i = index; i < NUM_GAME_ENTS; i++) {
		if (!ents[i]) continue;
		if (ents[i]->class_ == classtype) return i;
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
	if (!is_host)
		return;

	Ray r(origin, direction);
	RayHit hit = eng->phys.trace_ray(r, from->selfid, PF_ALL);
	if (hit.hit_world) {
		//local.pm.add_dust_hit(hit.pos-direction*0.1f);
		// add particles + decals here

		local.pm.add_blood_effect(hit.pos, -direction);
	}
	else if(hit.ent_id!=-1){
		Entity* hit_entitiy = eng->get_ent_from_handle(hit.ent_id);
		hit_entitiy->damage(from, direction, 100);

		local.pm.add_blood_effect(hit.pos, -direction);
	}

	create_grenade(from, origin + direction * 0.5f, direction);
}

Player* Game_Engine::get_client_player(int slot)
{
	ASSERT(slot < MAX_CLIENTS);
	if (!ents[slot]) return nullptr;
	ASSERT(ents[slot]->class_ == entityclass::PLAYER);
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
static float modulo_lerp(float start, float end, float mod, float alpha)
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
	bool bounce = flags & EF_BOUNCE;
	bool slide = flags & EF_SLIDE;

	velocity.y -= (12.f * eng->tick_interval);
	int bumps = 4;
	bool hit_a_wall = false;
	float time = eng->tick_interval;
	vec3 pre_velocity = velocity;
	glm::vec3 first_n;
	GeomContact contact{};
	for (int i = 0; i < bumps; i++) {
		vec3 end = position + velocity * (time / bumps);
		
		contact = eng->phys.trace_shape(Trace_Shape(end, col_radius), selfid, PF_WORLD);

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

void Entity::physics_update()
{
	switch (physics)
	{
	case EPHYS_MOVER:
		break;
	case EPHYS_PROJECTILE:
		projectile_physics();
		break;
	case EPHYS_GRAVITY:
		gravity_physics();
		break;
	}
}

void Entity::damage(Entity* attacker, glm::vec3 dir, int damage)
{
	if (flags & EF_DEAD)
		return;

	health -= damage;

	if (health <= 0) {
		flags |= EF_DEAD;
		timer = 5.f;

		anim.set_anim("act_die_1", true);
		anim.m.loop = false;
		anim.legs.anim = -1;
		flags &= ~EF_SOLID;

		flags |= EF_FORCED_ANIMATION;

		flags |= EF_FROZEN_VIEW;
	}
}

void player_spawn(Entity* ent)
{

	ent->set_model("player_FINAL.glb");

	if (ent->model) {
		ent->anim.set_anim("act_idle", false);
	}
	//server.sv_game.GetPlayerSpawnPoisiton(ent);
	ent->state = PMS_GROUND;
	ent->flags = 0;
	
	ent->flags |= EF_SOLID;

	ent->health = 100;
	find_spawn_position(ent);

	for (int i = 0; i < Game_Inventory::NUM_GAME_ITEMS; i++)
		ent->inv.ammo[i] = 200;

	ent->force_angles = 1;
	ent->diff_angles = glm::vec3(0.f, ent->rotation.y, 0.f);
}

Entity* dummy_spawn()
{
	Entity* ent = eng->create_entity(entityclass::NPC);
	ent->position = glm::vec3(0.f);
	ent->rotation = glm::vec3(0.f);
	ent->flags = 0;
	ent->health = 100;
	ent->set_model("player_character.glb");
	if (ent->model)
		ent->anim.set_anim("act_run", true);

	return ent;
}

void EntTakeDamage(Entity* ent, Entity* from, int amt)
{
	if (ent->flags & EF_DEAD)
		return;
	ent->health -= amt;
	if (ent->health <= 0) {
		ent->flags |= EF_DEAD;
		ent->timer = 3.0;

		ent->anim.set_anim("act_die", false);
		ent->anim.m.loop = false;

		printf("died!\n");
	}
}

#include "MeshBuilder.h"
#include "Config.h"
void Game_Engine::execute_player_move(int num, Move_Command cmd)
{
	double oldtime = eng->time;
	eng->time = cmd.tick * eng->tick_interval;
	
	Entity* ent = get_ent(num);
	player_physics_update(ent, cmd);
	player_post_physics(ent, cmd, num == player_num());

	eng->time = oldtime;
}

void Player::update()
{
	if (flags & EF_DEAD) {
		if (timer <= 0.f) {
			player_spawn(this);
		}
	}

	if (position.y < -50 && !(flags & EF_DEAD)) {
		flags |= EF_DEAD;
		timer = 0.5f;
	}

	if (viewmodel) {
		viewmodel->update();
	}
}

static Random fxrand = Random(1452345);

void grenade_hit_wall(Entity* ent, glm::vec3 normal)
{
	if (glm::length(ent->velocity)>1.f) {
		ent->rotation = vec3(fxrand.RandF(0, TWOPI), fxrand.RandF(0, TWOPI), fxrand.RandF(0, TWOPI));
		eng->local.pm.add_dust_hit(ent->position + normal * 0.1f);
	}
}

static Auto_Config_Var grenade_radius("game.gren_rad", 0.25f);
static Auto_Config_Var grenade_slide("game.gren_slide", 0);
static Auto_Config_Var grenade_vel("game.gren_vel", 18.f);




Grenade::Grenade()
{
	set_model("grenade_he.glb");
	flags = grenade_slide.integer() ? EF_SLIDE : EF_BOUNCE;
	physics = EPHYS_GRAVITY;
	col_radius = grenade_radius.real();
	timer = 5.f;
}
void Grenade::update()
{
	// spin grenade based on velocity
	float dt = eng->tick_interval;

	float vel = glm::length(velocity);

	if (timer <= 0.f) {
		sys_print("BOOM\n");
		eng->free_entity(selfid);
	}
}

Entity* create_grenade(Entity* thrower, glm::vec3 org, glm::vec3 direction)
{
	ASSERT(thrower);
	Grenade* e = (Grenade*)eng->create_entity(entityclass::THROWABLE);
	e->thrower = thrower->selfid;
	e->position = org;
	e->velocity = direction * grenade_vel.real();
	return e;
}