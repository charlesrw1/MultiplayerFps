// CameraPathFollower and CamPathFollowerLua implementations

#include "Player.h"
#include "GameEnginePublic.h"
#include "Game/GameplayStatic.h"
#include "Game/Components/CameraComponent.h"
#include "Game/Components/SpawnerComponenth.h"
#include "Framework/MathLib.h"

CameraPathFollower::CameraPathFollower(std::vector<SpawnerComponent*> components) {
	ASSERT(!components.empty());
	for (auto c : components) {
		points.push_back({c->get_ws_position(), c->get_owner()->get_ws_rotation()});
	}
	time_start = GetTime();
}

glm::vec3 CameraPathFollower::catmull_rom(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t) {
	ASSERT(t >= 0.0f && t <= 1.0f);
	return 0.5f * ((2.f * p1) + (-p0 + p2) * t + (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t * t +
				   (-p0 + 3.f * p1 - 3.f * p2 + p3) * t * t * t);
}

void CameraPathFollower::update() {
	ASSERT(!points.empty());
	double time = GetTime() - time_start;
	int index = (int)(time / time_per_point) % (int)points.size();
	int next  = (index + 1) % (int)points.size();
	int next2 = (index + 2) % (int)points.size();
	int next3 = (index + 3) % (int)points.size();
	(void)next2; (void)next3;

	float frac = (float)fmod(time, time_per_point) / (float)time_per_point;
	glm::quat rot = glm::slerp(points.at(index).q, points.at(next).q, frac);
	glm::vec3 pos = glm::mix(points.at(index).p, points.at(next).p, frac);

	CameraComponent::get_scene_camera()->get_owner()->set_ws_transform(pos, rot, glm::vec3(1));
}

void CamPathFollowerLua::update() {
	ASSERT(cur_idx >= -1);
	if (paths.empty())
		return;
	if (cur_idx < 0 || cur_idx >= (int)paths.size())
		cur_idx = 0;
	auto& path = paths.at(cur_idx);
	auto scene_cam = CameraComponent::get_scene_camera();
	if (!scene_cam)
		return;
	if (path.points.size() == 0) {
		goto_next();
		return;
	}
	if (path.points.size() == 1) {
		auto& p0 = path.points.at(0);
		scene_cam->get_owner()->set_ws_position_rotation(p0.pos, p0.rot);
	} else {
		float time_per_point = path.time / (float)(path.points.size() - 1);
		int first = (int)std::floor(cur_time / time_per_point);
		float frac = fmod(cur_time, time_per_point) / time_per_point;
		lTransform& p0 = path.points.at(first);
		lTransform& p1 = path.points.at(first + 1);
		scene_cam->get_owner()->set_ws_position_rotation(glm::mix(glm::vec3(p0.pos), glm::vec3(p1.pos), frac),
														 glm::slerp(glm::quat(p0.rot), glm::quat(p1.rot), frac));
		// GameplayStatic::debug_text(string_format("%i %.3f", first, frac));
	}
	float dt = GameplayStatic::get_dt();
	cur_time += dt;
	if (cur_time >= path.time) {
		goto_next();
	}
}
