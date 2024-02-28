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

Entity* ServerEntForIndex(int index)
{
	ASSERT(index >= 0 && index < MAX_GAME_ENTS);
	return &eng->ents[index];
}

void find_spawn_position(Entity* ent)
{
	int index = eng->find_by_classname(0, "player_spawn");
	bool found = false;
	while (index != -1) {
		Entity& e = eng->get_ent(index);

		// do a small test for nearby players
		GeomContact contact = eng->phys.trace_shape(Trace_Shape(e.position, CHAR_HITBOX_RADIUS), ent->index, PF_PLAYERS);

		if (!contact.found) {
			ent->position = e.position;
			ent->rotation = e.rotation;
			return;
		}
		
		index = eng->find_by_classname(index+1, "player_spawn");
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

void door_update(Entity* e)
{
	e->rotation.y = eng->time;
}

void spawn_door(Level::Entity_Spawn& spawn)
{
	Entity& e = *eng->new_entity();
	e.type = ET_USED;
	e.position = spawn.position;
	e.rotation = spawn.rotation;
	e.physics = EPHYS_MOVER;
	e.classname = "door";
	e.state = DOOR_CLOSED;
	e.flags = 0;
	e.flags |= EF_SOLID;
	e.update = door_update;

	for (auto kv : spawn.key_values) {
		if (kv.at(0) == "linked_mesh") {
			// this is unique, all other model_index setting goes through Game_Media
			int index = std::atoi(kv.at(1).c_str());
			e.model = eng->level->linked_meshes.at(index);
			e.model_index = index;
		}
	}



}
void spawn_spawn_point(Level::Entity_Spawn& spawn)
{
	Entity& e = *eng->new_entity();
	e.type = ET_USED;
	e.classname = "player_spawn";
	e.position = spawn.position;
	e.rotation = spawn.rotation;
	e.physics = EPHYS_NONE;


	for (auto kv : spawn.key_values) {
		if (kv.at(0) == "team") {
			int index = std::atoi(kv.at(1).c_str());
		}
	}
}

void spawn_zone(Level::Entity_Spawn& spawn)
{
	Entity& e = *eng->new_entity();
	e.type = ET_USED;
	if (spawn.classname == "bomb_area")
		e.classname = "bomb_area";
	else if (spawn.classname == "spawn_area")
		e.classname = "spawn_area";
	e.position = spawn.position;
	e.rotation = spawn.rotation;
	e.col_size = spawn.scale;


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
	Entity& ent = ents[slot];
	free_entity(&ent);
}

void Game_Engine::make_client(int slot)
{
	Entity& e = ents[slot];
	if (e.active()) sys_print("player slot %d already in use?\n", slot);
	e = Entity();
	e.index = slot;
	
	player_spawn(&e);
}

Entity* Game_Engine::new_entity()
{
	int slot = MAX_CLIENTS;
	for (; slot < MAX_GAME_ENTS; slot++) {
		if (ents[slot].type == ET_FREE)
			break;
	}
	if (slot == MAX_GAME_ENTS) return nullptr;
	num_entities++;
	Entity& e = ents[slot];
	e = Entity();
	e.type = ET_USED;	// hack
	e.index = slot;
	e.clear_pointers();
	return &e;
}

int Game_Engine::find_by_classname(int index, const char* classname)
{
	for (int i = index; i < MAX_GAME_ENTS; i++) {
		if (!ents[i].active()) continue;
		if (strcmp(ents[i].classname, classname) == 0) return i;
	}
	return -1;
}

void Game_Engine::free_entity(Entity* ent)
{
	*ent = Entity();
}

void Game_Engine::fire_bullet(Entity* from, vec3 direction, vec3 origin)
{
	if (!is_host)
		return;

	Ray r(origin, direction);
	RayHit hit = eng->phys.trace_ray(r, from->index, PF_ALL);
	if (hit.hit_world) {
		//local.pm.add_dust_hit(hit.pos-direction*0.1f);
		// add particles + decals here

		local.pm.add_blood_effect(hit.pos, -direction);
	}
	else if(hit.ent_id!=-1){
		Entity& hit_entitiy = eng->ents[hit.ent_id];
		if (hit_entitiy.damage)
			hit_entitiy.damage(&hit_entitiy, from, 100, 0);

		local.pm.add_blood_effect(hit.pos, -direction);
	}

	create_grenade(from, origin + direction * 0.5f, direction);
}

void Entity::clear_pointers()
{
	model = nullptr;
	hit_wall = nullptr;
	update = nullptr;
	damage = nullptr;
	touch = nullptr;
}
void Entity::add_to_last()
{
	Transform_Hist* l = &last[0];
	int index = 0;
	if (last[0].used) {
		if (last[1].used)
			l = &last[2];
		else
			l = &last[1];
	}
	l->used = true;
	l->o = position;
	l->r = rotation;
}
void Entity::shift_last()
{
	last[0] = last[1];
	last[1] = last[2];
	last[2].used = false;
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
	for (int i = 0; i < bumps; i++) {
		vec3 end = position + velocity * (time / bumps);
		
		GeomContact contact = eng->phys.trace_shape(Trace_Shape(end, col_radius), index, PF_WORLD);

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
		if (hit_wall)
			hit_wall(this, first_n);
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

void player_damage(Entity* self, Entity* attacker, int damage, int flags)
{
	if (self->flags & EF_DEAD)
		return;

	self->health -= damage;

	if (self->health <= 0) {
		self->flags |= EF_DEAD;
		self->timer = 5.f;

		self->anim.set_anim("act_die_1", true);
		self->anim.m.loop = false;
		self->anim.legs.anim = -1;
		self->flags &= ~EF_SOLID;

		self->flags |= EF_FORCED_ANIMATION;

		self->flags |= EF_FROZEN_VIEW;
	}
}

void player_spawn(Entity* ent)
{
	ent->type = ET_PLAYER;

	ent->set_model("player_character.glb");

	if (ent->model) {
		ent->anim.set_anim("act_idle", false);
	}
	//server.sv_game.GetPlayerSpawnPoisiton(ent);
	ent->state = PMS_GROUND;
	ent->flags = 0;
	
	ent->flags |= EF_SOLID;

	ent->health = 100;
	find_spawn_position(ent);

	ent->update = player_update;
	ent->damage = player_damage;

	for (int i = 0; i < Game_Inventory::NUM_GAME_ITEMS; i++)
		ent->inv.ammo[i] = 200;

	ent->force_angles = 1;
	ent->diff_angles = glm::vec3(0.f, ent->rotation.y, 0.f);
}

Entity* dummy_spawn()
{
	Entity* ent = eng->new_entity();
	ent->type = ET_DUMMY;
	ent->position = glm::vec3(0.f);
	ent->rotation = glm::vec3(0.f);
	ent->flags = 0;
	ent->health = 100;
	ent->set_model("player_character.glb");
	if (ent->model)
		ent->anim.set_anim("act_run", true);

	ent->update = dummy_update;

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
	
	Entity& ent = get_ent(num);
	player_physics_update(&ent, cmd);
	player_post_physics(&ent, cmd, num == player_num());

	eng->time = oldtime;
}

void player_update(Entity* ent)
{
	if (ent->flags & EF_DEAD) {
		if (ent->timer <= 0.f) {
			player_spawn(ent);
		}
	}

	if (ent->position.y < -50 && !(ent->flags & EF_DEAD)) {
		ent->flags |= EF_DEAD;
		ent->timer = 0.5f;
	}
}
void dummy_update(Entity* ent)
{
	//ent->position.y = sin(GetTime()) * 2.f + 2.f;
	ent->position.x = 0.f;
	if (ent->flags & EF_DEAD)
		eng->free_entity(ent);
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

Entity* create_grenade(Entity* thrower, glm::vec3 org, glm::vec3 direction)
{
	ASSERT(thrower);
	Entity* e = eng->new_entity();
	e->type = ET_GRENADE;
	e->set_model("grenade_he.glb");
	e->owner_index = thrower->index;
	e->position = org;
	e->velocity = direction * grenade_vel.real();
	e->flags = grenade_slide.integer() ? EF_SLIDE : EF_BOUNCE;
	e->timer = 5.f;
	e->physics = EPHYS_GRAVITY;
	e->col_radius = grenade_radius.real();
	e->hit_wall = grenade_hit_wall;
	e->update = grenade_update;
	return e;
}

void grenade_update(Entity* ent)
{
	// spin grenade based on velocity
	float dt = eng->tick_interval;

	float vel = glm::length(ent->velocity);

	if (ent->timer <= 0.f) {
		sys_print("BOOM\n");
		eng->free_entity(ent);
	}
}
