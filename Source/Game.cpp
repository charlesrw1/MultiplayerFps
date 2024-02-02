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
	return &engine.ents[index];
}

void GetPlayerSpawnPoisiton(Entity* ent)
{
	if (engine.level->spawns.size() > 0) {
		ent->position = engine.level->spawns[0].position;
		ent->rotation.y = engine.level->spawns[0].angle;
	}
	else {
		ent->position = glm::vec3(0);
		ent->rotation = glm::vec3(0);
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

void Game_Engine::free_entity(Entity* ent)
{
	*ent = Entity();
}

void Game_Engine::fire_bullet(Entity* from, vec3 direction, vec3 origin)
{
	if (!is_host)
		return;

	Ray r(origin, direction);
	RayHit hit = engine.phys.trace_ray(r, from->index, PF_ALL);
	if (hit.hit_world) {
		local.pm.add_dust_hit(hit.pos-direction*0.1f);
		// add particles + decals here
	}
	else if(hit.ent_id!=-1){
		Entity& hit_entitiy = engine.ents[hit.ent_id];
		if (hit_entitiy.damage)
			hit_entitiy.damage(&hit_entitiy, from, 100, 0);
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

void Entity::projectile_physics()
{
	for (int i = 0; i < 5; i++) {

	}
}
void Entity::gravity_physics()
{
	bool bounce = flags & EF_BOUNCE;
	bool slide = flags & EF_SLIDE;

	velocity.y -= (12.f * engine.tick_interval);
	int iters = 8;
	vec3 new_pos = position;
	vec3 delta = (float)engine.tick_interval*velocity / (float)iters;
	bool called_func = false;
	for (int i = 0; i < iters; i++) {
		new_pos += delta;
		
		GeomContact contact = engine.phys.trace_shape(Trace_Shape(new_pos, col_radius), index, PF_WORLD);

		if (contact.found) {
			new_pos += contact.penetration_normal * (contact.penetration_depth);

			if (bounce) {
				velocity = glm::reflect(velocity, contact.surf_normal);
				velocity *= 0.6f;
			}
			else if (slide) {
				vec3 penetration_velocity = dot(velocity, contact.penetration_normal) * contact.penetration_normal;
				vec3 slide_velocity = velocity - penetration_velocity;
				velocity = slide_velocity;
			}
			else {
				velocity = vec3(0.f);
			}

			if (bounce && !called_func && hit_wall) {
				hit_wall(this, contact.surf_normal);
				called_func = true;
			}
			break;
		}
	}
	position = new_pos;
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

		self->flags |= EF_FORCED_ANIMATION;
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

	ent->health = 100;
	GetPlayerSpawnPoisiton(ent);

	ent->update = player_update;
	ent->damage = player_damage;
}

Entity* dummy_spawn()
{
	Entity* ent = engine.new_entity();
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
	double oldtime = engine.time;
	engine.time = cmd.tick * engine.tick_interval;
	
	Entity& ent = engine.ents[num];
	player_physics_update(&ent, cmd);
	player_post_physics(&ent, cmd);

	engine.time = oldtime;
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
		engine.free_entity(ent);
}

static Random fxrand = Random(1452345);

void grenade_hit_wall(Entity* ent, glm::vec3 normal)
{
	if (glm::length(ent->velocity)>1.f) {
		ent->rotation = vec3(fxrand.RandF(0, TWOPI), fxrand.RandF(0, TWOPI), fxrand.RandF(0, TWOPI));
		engine.local.pm.add_dust_hit(ent->position + normal * 0.1f);
	}
}

Entity* create_grenade(Entity* thrower, glm::vec3 org, glm::vec3 direction)
{
	static Config_Var* grenade_radius = cfg.get_var("game/grenade_radius", "0.25f");
	static Config_Var* grenade_slide = cfg.get_var("game/grenade_slide", "0");
	static Config_Var* grenade_vel = cfg.get_var("game/grenade_vel", "18.0");

	ASSERT(thrower);
	Entity* e = engine.new_entity();
	e->type = ET_GRENADE;
	e->set_model("grenade_he.glb");
	e->owner_index = thrower->index;
	e->position = org;
	e->velocity = direction * grenade_vel->real;
	e->flags = grenade_slide->integer ? EF_SLIDE : EF_BOUNCE;
	e->timer = 5.f;
	e->physics = EPHYS_GRAVITY;
	e->col_radius = grenade_radius->real;
	e->hit_wall = grenade_hit_wall;
	e->update = grenade_update;
	return e;
}

void grenade_update(Entity* ent)
{
	// spin grenade based on velocity
	float dt = engine.tick_interval;

	float vel = glm::length(ent->velocity);

	if (ent->timer <= 0.f) {
		sys_print("BOOM\n");
		engine.free_entity(ent);
	}
}
