#pragma once

#include <cstdint>
#include "glm/glm.hpp"
#include "RenderObj.h"

#include "BaseComponents.h"
#include "Interaction.h"

#include "Framework/TypeInfo.h"
#include "Framework/AddClassToFactory.h"
#include "Framework/StringName.h"
#include "Framework/ClassBase.h"

class Model;
class GeomContact;


struct EntityFlags
{
	enum Enum
	{
		Dead = 1,
		Hidden = 2,
	};
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

class PhysicsActor;
class Dict;

CLASS_H(Entity, ClassBase)

	Entity();
	virtual ~Entity();

	// initialize values
	virtual void spawn(const Dict& spawn_args);
	// initialize anything that depends on references to other entities
	virtual void post_spawn(const Dict& spawn_args) {}
	virtual void update();
	virtual void present();

	bool has_physics_actor() const { return physics_actor; }

	entityhandle selfid = 0;	// eng->ents[]
	std::string name_id;		// name of entity frome editor

	PhysicsActor* physics_actor = nullptr;
	glm::vec3 scale = glm::vec3(1.f);
	glm::vec3 position = glm::vec3(0.0);
	glm::quat rotation = glm::quat();
	glm::vec3 esimated_accel = glm::vec3(0.f);

	EntityFlags::Enum flags = EntityFlags::Enum(0);
	int health = 100;
	
	// fix this garbo
	Game_Inventory inv;

	handle<Render_Object> render_handle;
	Render_Object renderable;

	virtual AnimatorInstance* get_animator() { return nullptr; }

	void set_model(const char* model);

	virtual glm::vec3 get_velocity() const {
		return glm::vec3(0.f);
	}

	void move();
	void projectile_physics();
	void gravity_physics();
	void mover_physics();


	bool has_flag(EntityFlags::Enum flag) const {
		return flags & flag;
	}
	void set_flag(EntityFlags::Enum flag, bool val) {
		if (val)
			flags = EntityFlags::Enum(flags | flag);
		else
			flags = EntityFlags::Enum(flags & (~flag));
	}
	glm::mat4 get_world_transform();
	bool can_render_this_object() const {
		return renderable.model && !is_object_hidden();
	}
	bool is_object_hidden() const {
		return flags & EntityFlags::Hidden;
	}
	Model* get_model() const { return renderable.model; }
};
