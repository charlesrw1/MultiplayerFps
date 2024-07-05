#pragma once
#include "Types.h"
#include "Game_Engine.h"

#include "Entity.h"
#include "Interaction.h"
#include "Framework/Dict.h"



using namespace glm;
CLASS_H(NPC, Entity)

	NPC() {
		set_model("player_FINAL.glb");
		pathfind_state = going_towards_waypoint;
		position = vec3(0.f);

		rotation.y = HALFPI;
	}
	glm::vec3 velocity = glm::vec3(0.f);

	virtual void spawn(const Dict& spawnargs) override;
	virtual void update() override {

		glm::vec3 waypoints[3] = {
			glm::vec3(4.8, -0.674, -1.7),
			glm::vec3(-2.5, -0.67f,-1.3),
			glm::vec3(-3.3,-0.67, 6.78)
		};

		glm::vec3 prevel = velocity;

		float speed = length(velocity);
		if (speed >= 0.0001) {
			float dropamt = ground_friction * speed * eng->tick_interval;
			float newspd = speed - dropamt;
			if (newspd < 0)
				newspd = 0;
			float factor = newspd / speed;
			velocity.x *= factor;
			velocity.z *= factor;
		} 
		float dt = eng->tick_interval;
		switch (pathfind_state)
		{
		case going_towards_waypoint: {
			//waypoints[current_waypoint] = eng->local_player().position;
			glm::vec3 towaypoint = waypoints[current_waypoint] - position;
			glm::vec3 grnd_face_dir = glm::normalize( vec3(towaypoint.x, 0, towaypoint.z) );
			float speed = 5.5f;
			vec3 idealvelocity = normalize(towaypoint)*speed;
			vec3 add_vel_dir = idealvelocity - velocity;
			if (!VarMan::get()->find("stopai")->get_float()) {
				float lenavd = length(add_vel_dir);
				if (lenavd >= 0.000001)
					velocity += add_vel_dir / lenavd * ground_accel * speed * (dt);
				else
					velocity += grnd_face_dir * ground_accel * speed * (dt);
			}
			position += velocity*dt;

			float len = glm::length(position - waypoints[current_waypoint]);
			if (len <= 1.f) {
				current_waypoint = (current_waypoint + 1) % 3;
				pathfind_state = turning_to_next_waypoint;
			}

			RayHit rh = eng->phys.trace_ray(Ray(position + vec3(0, 2, 0), glm::vec3(0, -1, 0)),-1,PF_WORLD);
			if (rh.dist > 0) position.y = rh.pos.y;

		}break;

		case waiting_at_waypoint: {

		}break;

		case turning_to_next_waypoint: {
			pathfind_state = going_towards_waypoint;
		}break;
		};
		esimated_accel =(velocity - prevel) / (float)eng->tick_interval;
	}

	enum state {
		going_towards_waypoint,
		waiting_at_waypoint,
		turning_to_next_waypoint
	}pathfind_state;

	void update_animation_inputs() {
		
	}

	bool strafe_only = true;
	float ground_accel = 6.5f;
	float ground_friction = 8.f;

	int current_waypoint = 0;
};

CLASS_H(Door, Entity)

	enum {
		OPEN,
		CLOSED
	}doorstate;

	void spawn(const Dict& spawnargs) override;
	void update() override;
};

CLASS_H(Grenade, Entity)

	Grenade();

	void set_thrower(entityhandle handle) {
		thrower = handle;
	}

	void spawn(const Dict& spawnargs) override;
	void update() override;

	float throw_time = 0.0;

	entityhandle thrower = -1;
};
