#pragma once

#include "Types.h"
#include "GameEnginePublic.h"

#include "Game/Entity.h"

#include "Game/Components/MeshComponent.h"
#include "Game/Components/PhysicsComponents.h"


using namespace glm;
CLASS_H(NPC, Entity)
public:
	NPC() {
		pathfind_state = going_towards_waypoint;
		//position = vec3(0.f);
		//rotation.y = HALFPI;

		npc_model = construct_sub_component<MeshComponent>("NpcModel");
		npc_hitbox = construct_sub_component<CapsuleComponent>("NpcCollider");
	}

	static const PropertyInfoList* get_props() = delete;

	glm::vec3 velocity = glm::vec3(0.f);

	CapsuleComponent* npc_hitbox{};
	MeshComponent* npc_model{};

	virtual void update() override {

		glm::vec3 waypoints[3] = {
			glm::vec3(4.8, -0.674, -1.7),
			glm::vec3(-2.5, -0.67f,-1.3),
			glm::vec3(-3.3,-0.67, 6.78)
		};

		glm::vec3 prevel = velocity;

		float speed = length(velocity);
		if (speed >= 0.0001) {
			float dropamt = ground_friction * speed * eng->get_tick_interval();
			float newspd = speed - dropamt;
			if (newspd < 0)
				newspd = 0;
			float factor = newspd / speed;
			velocity.x *= factor;
			velocity.z *= factor;
		}
		float dt = eng->get_tick_interval();
		switch (pathfind_state)
		{
		case going_towards_waypoint: {
			//waypoints[current_waypoint] = eng->local_player().position;
			glm::vec3 towaypoint = waypoints[current_waypoint];// -position;
			glm::vec3 grnd_face_dir = glm::normalize(vec3(towaypoint.x, 0, towaypoint.z));
			float speed = 5.5f;
			vec3 idealvelocity = normalize(towaypoint) * speed;
			vec3 add_vel_dir = idealvelocity - velocity;
			if (!VarMan::get()->find("stopai")->get_float()) {
				float lenavd = length(add_vel_dir);
				if (lenavd >= 0.000001)
					velocity += add_vel_dir / lenavd * ground_accel * speed * (dt);
				else
					velocity += grnd_face_dir * ground_accel * speed * (dt);
			}
			//position += velocity * dt;

			float len = 0;// glm::length(position - waypoints[current_waypoint]);
			if (len <= 1.f) {
				current_waypoint = (current_waypoint + 1) % 3;
				pathfind_state = turning_to_next_waypoint;
			}

			//RayHit rh = eng->phys.trace_ray(Ray(position + vec3(0, 2, 0), glm::vec3(0, -1, 0)),-1,PF_WORLD);
			//if (rh.dist > 0) position.y = rh.pos.y;

		}break;

		case waiting_at_waypoint: {

		}break;

		case turning_to_next_waypoint: {
			pathfind_state = going_towards_waypoint;
		}break;
		};
		//esimated_accel = (velocity - prevel) / (float)eng->get_tick_interval();
	}

	enum state {
		going_towards_waypoint,
		waiting_at_waypoint,
		turning_to_next_waypoint
	}pathfind_state;


	bool strafe_only = true;
	float ground_accel = 6.5f;
	float ground_friction = 8.f;

	int current_waypoint = 0;
};
#include "Assets/AssetDatabase.h"
#include "Render/Model.h"
CLASS_H(Door, Entity)
public:
	Door() {
		door_mesh = construct_sub_component<MeshComponent>("DoorMesh");

		door_mesh->set_model(GetAssets().find_assetptr_unsafe<Model>("door.cmdl"));
	}


	bool start_open = false;
	bool start_locked = false;

	MeshComponent* door_mesh = nullptr;
	MeshComponent* door_handle = nullptr;

	static const PropertyInfoList* get_props() {
		START_PROPS(Door)
			REG_BOOL(start_locked, PROP_DEFAULT, ""),
			REG_BOOL(start_open, PROP_DEFAULT, ""),
		END_PROPS(Door)

	}
	void start() override {
		tickEnabled = true;	// tick me
	}
	void update() override;
};

CLASS_H(Grenade, Entity)
public:
	Grenade();



	void update() override;

	float throw_time = 0.0;

};
