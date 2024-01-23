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
void GrenadeUpdate(Entity* ent);


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
	printf("removing client %d from game\n", slot);
	Entity& ent = ents[slot];
	free_entity(&ent);
}

void Game_Engine::make_client(int slot)
{
	console_printf("Spawning client %d into game\n", slot);
	Entity& e = ents[slot];
	if (e.active()) printf("player slot %d already in use?\n", slot);
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
	RayHit hit;
	engine.phys.TraceRay(r, &hit, from->index, PF_ALL);
	if (hit.hit_world) {
		// add particles + decals here
	}
	else if(hit.ent_id!=-1){
		Entity& hit_entitiy = engine.ents[hit.ent_id];
		if (hit_entitiy.damage)
			hit_entitiy.damage(&hit_entitiy, from, 100, 0);
	}

	create_grenade(from, origin + direction * 0.1f, direction * 18.f);
}


void player_damage(Entity* self, Entity* attacker, int damage, int flags)
{
	if (self->flags & EF_DEAD)
		return;

	self->health -= damage;
	if (self->health <= 0) {
		self->flags |= EF_DEAD;
		self->death_time = engine.time+5.f;

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
		ent->death_time = engine.time + 3.0;

		ent->anim.set_anim("act_die", false);
		ent->anim.m.loop = false;

		printf("died!\n");
	}
}


void Entity::from_entity_state(EntityState& es)
{
	type = (Ent_Type)es.type;
	position = es.position;
	rotation = es.angles;
	model_index = es.model_idx;
	item = es.item;
	solid = es.solid;

	// update model
	if (model_index != -1)
		model = media.get_game_model_from_index(model_index);
	if(model&&model->bones.size()>0)
		anim.set_model(model);

	// animation set when lerping

	flags = es.flags;
}

PlayerState Entity::ToPlayerState() const
{
	PlayerState ps{};
	ps.position = position;
	ps.angles = rotation;
	ps.velocity = velocity;

	ps.state = state;

	ps.items = items;
	return ps;
}
void Entity::FromPlayerState(PlayerState* ps)
{
	position = ps->position;
	rotation = ps->angles;
	velocity = ps->velocity;

	state = ps->state;

	items = ps->items;
}
EntityState Entity::to_entity_state()
{
	EntityState es;
	es.type = type;
	es.position = position;
	es.angles = rotation;
	es.model_idx = model_index;
	es.solid = solid;

	es.item = items.active_item;
	es.flags = flags;

	es.leganim = anim.legs.anim;
	es.leganim_frame = anim.legs.frame;
	es.legblend = anim.legs.blend_anim;
	es.leganim_frame = anim.legs.blend_frame;
	es.legblendleft = anim.legs.blend_remaining;
	es.legsblendtime = anim.legs.blend_time;

	es.mainanim = anim.m.anim;
	es.mainanim_frame = anim.m.frame;
	es.torsoblend = anim.m.blend_anim;
	es.torsoblendframe = anim.m.blend_frame;
	es.torsoblendleft = anim.m.blend_remaining;
	es.torsoblendtime = anim.m.blend_time;


	return es;
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
		if (ent->death_time < engine.time) {
			player_spawn(ent);
		}
	}

	if (ent->position.y < -50 && !(ent->flags & EF_DEAD)) {
		ent->flags |= EF_DEAD;
		ent->death_time = engine.time + 0.5f;
	}
}
void dummy_update(Entity* ent)
{
	//ent->position.y = sin(GetTime()) * 2.f + 2.f;
	ent->position.x = 0.f;
	if (ent->flags & EF_DEAD)
		engine.free_entity(ent);
}

Entity* create_grenade(Entity* thrower, glm::vec3 org, glm::vec3 start_vel)
{
	ASSERT(thrower);
	Entity* e = engine.new_entity();
	e->type = ET_GRENADE;
	e->set_model("grenade_he.glb");
	e->owner_index = thrower->index;
	e->position = org;
	e->velocity = start_vel;
	e->flags = 0;
	e->death_time = engine.time + 5.f;

	e->update = GrenadeUpdate;
	return e;
}

void RunProjectilePhysics(Entity* ent)
{
	// update physics, detonate if ready
	float dt = engine.tick_interval;
	ent->velocity.y -= 12.f * dt;// gravity
	glm::vec3 next_position = ent->position + ent->velocity * dt;
	float len = glm::length(ent->velocity * dt);
	RayHit rh;
	Ray r;
	r.dir = (ent->velocity * dt) / len;
	r.pos = ent->position;
	engine.phys.TraceRay(r, &rh, ent->owner_index, PF_ALL);
	//g->RayWorldIntersect(r, &rh, ent->owner_index, PF_ALL);
	if (rh.hit_world && rh.dist < len) {
		ent->position = r.at(rh.dist) + rh.normal * 0.01f;
		ent->velocity = glm::reflect(ent->velocity, rh.normal);
		ent->velocity *= 0.6f;
	}
	else {
		ent->position = next_position;
	}
}


void GrenadeUpdate(Entity* ent)
{
	RunProjectilePhysics(ent);
	// spin grenade based on velocity
	float dt = engine.tick_interval;

	float vel = glm::length(ent->velocity);

	ent->rotation.x += 0.7 * dt * vel;
	ent->rotation.y -= 1.3 * dt * vel;

	if (ent->death_time < engine.time) {
		printf("BOOM\n");
		engine.free_entity(ent);
	}
}
