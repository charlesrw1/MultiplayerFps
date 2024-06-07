#pragma once

#include <cstdint>
#include "glm/glm.hpp"
#include "RenderObj.h"
#include "Animation/Runtime/Animation.h"
#include "BaseComponents.h"
#include "Interaction.h"

#include "Framework/TypeInfo.h"
#include "Framework/AddClassToFactory.h"
#include "Framework/StringName.h"


class Model;
class GeomContact;

enum Entity_Flags
{
	EF_DEAD = 1,
	EF_HIDDEN = 2,
	EF_HIDE_ITEM = 4,
	EF_DISABLE_PHYSICS = 8,
	EF_FROZEN_VIEW = 16,
};

enum class Ent_PhysicsType : uint8_t
{
	None,				// dont do any physics calculations
	Solid,				// None, but pushes away other physics objects
	Passthrough,		// None, but checks overlaps, for triggers etc.
	Simple,				// position+velocity integration with gravity
	SimpleNoGravity,	// Simple but no gravity
	Complex,			// Simple but with collision
	PlayerMove,			// Only for players

	RigidBody,			// use rigid body sim TODO :)
};

enum class Ent_PhysicsShape : uint8_t
{
	AABB,
	Sphere,
};

struct Ent_PhysicsSettings
{
	Ent_PhysicsType type = Ent_PhysicsType::None;
	Ent_PhysicsShape shape = Ent_PhysicsShape::AABB;
	float friction = 0.0;
	float grav_scale = 1.0;
	float elasticity = 0.0;
	glm::vec3 size = glm::vec3(0.f);

	float get_radius() const { return size.x; }
	bool is_solid() const {
		return type == Ent_PhysicsType::Solid;
	}
};

typedef uint32_t entityhandle;

class Entity;
class EntityRef
{
public:
	EntityRef() : index(0), guid(0) {}
	explicit EntityRef(const Entity* e) {}
	EntityRef(uint32_t index, uint32_t guid) : index(index), guid(guid) {}

	Entity* get() const;
	void print() const;
private:
	uint32_t index : 20;
	uint32_t guid : 12;
};

#define ENTITY_HEADER()\
	const TypeInfo get_typeinfo() const override;

#define ENTITY_IMPL(classname) \
	const TypeInfo classname::get_typeinfo() const { \
			return {#classname, sizeof(classname) };\
	}\
	static AddClassToFactory<classname,Entity> facimpl##classname(get_entityfactory(), #classname);

enum class GenericAttachment_e : uint8_t
{
	Mesh,
	Fx,
	Light,
	Decal,
};

class GenericAttachment_t
{
public:
	bool is_attached_to_bone() const { return bone != 0xff; }
	int get_bone() const { return bone; }
	GenericAttachment_e get_type() const { return type; }
	// returns false if should be removed
	bool update();

	void create_render();
	void create_fx();
	void create_light();
	void create_decal();

	union {
		int render_handle = -1;
		int fx_handle;
		int light_handle;
		int decal_handle;
	};
	uint16_t custom = 0;
	GenericAttachment_e type = GenericAttachment_e::Mesh;
	uint8_t bone = 0xff;
	glm::mat4x3 transform = glm::mat4(1.0);
};


enum class call_type_e : uint8_t
{
	event_on_handle,
	event_on_self,
	event_on_activator,
};

struct RegisteredEvent
{
	StringName ev_name;
	call_type_e call_type = call_type_e::event_on_handle;
	entityhandle call_this_on;
	float delay = 0.0;
	RegisteredEvent* next = nullptr;
};


// list of observers that are connected to signals, such as OnTrigger, OnTouch, OnPlayerKilled etc.
// these then either send an event to an entity or broadcast it globally
class SignalObservers
{
public:
	void post_signal(StringName name, entityhandle handle);
	std::vector<RegisteredEvent> signal_map;
};

struct EntInputEvent
{
	// args
	// entityhandle to call on
	// entityhandle that kicked off the chain
};


// specific input commands like Damage, Kill, Open, Close, Play, Pause, etc. that can be queued up, or called immeadeatley
class EntityEventBus
{
public:
};

struct UpdateFlags
{
	enum Enum : uint8_t
	{
		UPDATE = 1,	// pre physics
		PRESENT = 2,	// post physics
	};
};


class Entity
{
public:
	Entity();
	virtual ~Entity();
	virtual const TypeInfo get_typeinfo() const = 0;
	StringName get_classname() const {
		// FIXME: this
		return StringName(get_typeinfo().name);
	}

	virtual void spawn();
	virtual void update();
	virtual void present();

	virtual InlineVec<Interaction*, 2> get_interactions() const { return {}; };

	entityhandle selfid = 0;	// eng->ents[]
	StringName self_name;		// name of entity to identify them

	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	glm::vec3 velocity = glm::vec3(0);
	glm::vec3 esimated_accel = glm::vec3(0.f);
	Ent_PhysicsSettings phys_opt;

	uint32_t state_flags = 0;	// Entity flags
	int health = 100;
	
	// fix this garbo
	Game_Inventory inv;

	handle<Render_Object> render_handle;
	Model* model = nullptr;

	unique_ptr<Animator> animator;

	void set_model(const char* model);
	void initialize_animator(
		const Animation_Tree_CFG* graph, 
		IAnimationGraphDriver* driver = nullptr);

	void move();
	void projectile_physics();
	void gravity_physics();
	void mover_physics();

	glm::mat4 get_world_transform();

	bool is_solid() const { return phys_opt.is_solid(); }

	// transform heirarchy
	entityhandle parent = -1;
	entityhandle group_next = -1;	// next link in current master parent tree
	vector<GenericAttachment_t> attachments;	// cheap way to add lights/decals/etc. to an entities transform heirarchy
};

template<typename K, typename T>
class Factory;
extern Factory<std::string, Entity>& get_entityfactory();
