#include "Game_Engine.h"
#include "Client.h"
#include "Util.h"
#include "Game_Engine.h"
#include "GlmInclude.h"
#include "Player.h"
#include "MeshBuilder.h"
#include "Config.h"


glm::vec3 GetRecoilAmtTriangle(glm::vec3 maxrecoil, float t, float peakt)
{
	float p = (1 / (peakt - 1));

	if (t < peakt)
		return maxrecoil * (1/peakt)* t;
	else
		return maxrecoil * (p * t - p);

}


void Game_Local::update_viewmodel()
{
	Entity& e = eng->local_player();
	glm::vec3 view_front = AnglesToVector(eng->local.view_angles.x, eng->local.view_angles.y);
	view_front.y = 0;
	glm::vec3 side_grnd = glm::normalize(glm::cross(view_front, vec3(0, 1, 0)));
	float spd_side = dot(side_grnd, e.velocity);
	float side_ofs_ideal = -spd_side / 200.f;
	glm::clamp(side_ofs_ideal, -0.007f, 0.007f);
	float spd_front = dot(view_front, e.velocity);
	float front_ofs_ideal = spd_front / 200.f;
	glm::clamp(front_ofs_ideal, -0.007f, 0.007f);
	float up_spd = e.velocity.y;
	float up_ofs_ideal = -up_spd / 200.f;
	glm::clamp(up_ofs_ideal, -0.007f, 0.007f);

	if (e.state & PMS_CROUCHING)
		up_ofs_ideal += 0.04;

	viewmodel_offsets = damp(viewmodel_offsets, vec3(side_ofs_ideal, up_ofs_ideal, front_ofs_ideal), 0.01f, eng->frame_time * 100.f);

	//viewmodel_offsets = glm::mix(viewmodel_offsets, vec3(side_ofs_ideal, up_ofs_ideal, front_ofs_ideal), 0.4f);

	if (e.inv.state != prev_item_state)
	{
		switch (e.inv.state)
		{
		case ITEM_IDLE:
			viewmodel_recoil_ofs = viewmodel_recoil_ang = glm::vec3(0.f);
			vm_recoil_end_time = vm_recoil_start_time = 0.f;
			break;
		case ITEM_IN_FIRE:
			vm_recoil_start_time = eng->time;
			vm_recoil_end_time = e.inv.timer;	// FIXME: read from current item data
			break;
		case ITEM_RELOAD:
			break;

		}

		prev_item_state = e.inv.state;
	}
	switch (e.inv.state)
	{
	case ITEM_IN_FIRE: {
		float end = e.inv.timer;
		if (end > vm_recoil_end_time) {
			vm_recoil_end_time = end;
			vm_recoil_start_time = eng->time;
		}

		float t = (eng->time - vm_recoil_start_time) / (vm_recoil_end_time - vm_recoil_start_time);
		t = glm::clamp(t, 0.f, 1.f);
		viewmodel_recoil_ofs = GetRecoilAmtTriangle(vec3(0.0, 0, 0.3), t, 0.4f);
	}break;
	}

	viewmodel_animator.AdvanceFrame(eng->frame_time);
	viewmodel_animator.SetupBones();
	viewmodel_animator.ConcatWithInvPose();

}