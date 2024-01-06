#include "Client.h"
#include "Util.h"
#include "CoreTypes.h"
#include "GlmInclude.h"
#include "Movement.h"
#include "MeshBuilder.h"
#include "Config.h"
#include "GameData.h"

bool Client::IsInGame() const
{
	return GetConState() == Spawned;
}

ClientGame::ClientGame() : rand(time(NULL)) {

}

ClientEntity* Client::GetLocalPlayer()
{
	ASSERT(IsInGame());
	int index = GetPlayerNum();
	return &cl_game.entities[index];
}

void ClientGame::Init()
{
	entities.resize(MAX_GAME_ENTS);
	level = nullptr;
	particles.Init(this, &client);
}
void ClientGame::ClearState()
{
	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		entities[i].active = false;
		entities[i].ClearState();
	}
	phys.ClearObjs();
	particles.ClearAll();

	if(level)
		FreeLevel(level);
	level = nullptr;
}
void ClientGame::NewMap(const char* mapname)
{
	if (level) {
		printf("Client: freeing level\n");
		FreeLevel(level);
		level = nullptr;
	}
	ClearState();	// cleansup game state
	level = LoadLevelFile(mapname);
}

void ClientGame::ComputeAnimationMatricies()
{
	for (int i = 0; i < entities.size(); i++) {
		ClientEntity& ce = entities[i];
		if (!ce.active)
			continue;
		if (!ce.model || !ce.model->animations)
			continue;

		// TEMP:
		ce.animator.Init(ce.model);
		ce.animator.ResetLayers();
		ce.animator.mainanim = ce.interpstate.leganim;
		ce.animator.mainanim_frame = ce.interpstate.leganim_frame;

		ce.animator.SetupBones();
		ce.animator.ConcatWithInvPose();
	}
}

void ClientGame::InterpolateEntStates()
{
	double rendering_time = client.tick * core.tick_interval - (client.cfg_interp_time->real);
	for (int i = 0; i < entities.size(); i++) {
		if (entities[i].active && i != client.GetPlayerNum()) {
			entities[i].InterpolateState(rendering_time, core.tick_interval);
		}
	}

	double rendering_time_client = client.tick * core.tick_interval - core.frame_remainder;
	ClientEntity* local = client.GetLocalPlayer();
	local->interpstate = local->GetLastState()->state;
}


void ClientSetViewmodelCallback(const char* str)
{
	/* null */
}
void ClientGameShootCallback(int entindex, bool altfire)
{
	/* null */
	printf("client: shoot\n");
}
void ClientPlaySoundCallback(vec3 org, int snd_idx)
{
	/* null */
}

ByteWriter GetGameeventBuffer()
{
	static uint8_t bytes[1500];
	ByteWriter bw(bytes, 1500);
	return bw;
}


void MakeSoundEvent(vec3 loc, int soundidx)
{
	ByteWriter bw = GetGameeventBuffer();

	bw.WriteByte(Ev_Sound);
	bw.WriteShort(soundidx);
	bw.WriteShort((loc.x) * 20.f);
	bw.WriteShort((loc.y) * 20.f);
	bw.WriteShort((loc.z) * 20.f);
}

void MakeDecalEvent(vec3 loc, int decalidx)
{
	ByteWriter bw = GetGameeventBuffer();

	bw.WriteByte(Ev_Sound);
	bw.WriteShort(decalidx);
	bw.WriteShort((loc.x) * 20.f);
	bw.WriteShort((loc.y) * 20.f);
	bw.WriteShort((loc.z) * 20.f);
}

void MakeBulletEvent(vec3 start, vec3 end, bool tracer)
{
	ByteWriter bw = GetGameeventBuffer();
	bw.WriteByte(Ev_FirePrimary);
	bw.WriteByte(tracer);
	bw.WriteShort((start.x) * 20.f);
}

void ClientGame::BuildPhysicsWorld()
{
	phys.ClearObjs();
	phys.AddLevel(level);
	
	for (int i = 0; i < entities.size(); i++) {
		ClientEntity& ce = entities[i];
		if (!ce.active) continue;
		
		StateEntry* s = ce.GetLastState();
		if (s->state.type != Ent_Player)
			continue;

		CharacterShape cs;
		cs.a = &ce.animator;
		cs.m = ce.model;
		cs.org = s->state.position;
		cs.radius = CHAR_HITBOX_RADIUS;
		cs.height = (!s->state.ducking) ? CHAR_STANDING_HB_HEIGHT : CHAR_CROUCING_HB_HEIGHT;
		PhysicsObject po;
		po.shape = PhysicsObject::Character;
		po.character = cs;
		po.userindex = i;
		po.player = true;

		phys.AddObj(po);
	}
}

void ClientGame::RunCommand(const PlayerState* in, PlayerState* out, MoveCommand cmd, bool run_fx)
{
	MeshBuilder b;

	PlayerMovement move;
	move.cmd = cmd;
	move.deltat = core.tick_interval;
	move.phys_debug = &b;
	move.player = *in;
	move.max_ground_speed = cfg.find_var("max_ground_speed")->real;
	move.simtime = cmd.tick * core.tick_interval;
	move.isclient = true;
	move.phys = &phys;
	move.fire_weapon = ClientGameShootCallback;
	move.set_viewmodel_animation = ClientSetViewmodelCallback;
	move.play_sound = ClientPlaySoundCallback;
	move.entindex = client.GetPlayerNum();
	move.Run();

	*out = move.player;
	
	if (run_fx) {
		view_recoil.x += move.view_recoil_add.x;
	}

}

glm::vec3 GetRecoilAmtTriangle(glm::vec3 maxrecoil, float t, float peakt)
{
	float p = (1 / (peakt - 1));

	if (t < peakt)
		return maxrecoil * (1/peakt)* t;
	else
		return maxrecoil * (p * t - p);

}


void ClientGame::PreRenderUpdate()
{ 
	particles.Update(core.frame_time);
	InterpolateEntStates();
	ComputeAnimationMatricies();

	UpdateViewmodelAnimation();
	UpdateViewModelOffsets();

	UpdateCamera();
}

void ClientGame::UpdateViewmodelAnimation()
{
	PlayerState* p = &client.lastpredicted;
	if (p->items.state != prev_item_state)
	{
		switch (p->items.state)
		{
		case Item_Idle:
			viewmodel_recoil_ofs = viewmodel_recoil_ang = glm::vec3(0.f);
			vm_recoil_end_time = vm_recoil_start_time = 0.f;
			break;
		case Item_InFire:
			vm_recoil_start_time = client.time;
			vm_recoil_end_time = p->items.gun_timer;	// FIXME: read from current item data
			break;
		case Item_Reload:
			break;

		}

		prev_item_state = p->items.state;
	}
	switch (p->items.state)
	{
	case Item_InFire: {
		float end = p->items.gun_timer;
		if (end > vm_recoil_end_time) {
			vm_recoil_end_time = end;
			vm_recoil_start_time = client.time;
		}

		float t = (client.time - vm_recoil_start_time)/ (vm_recoil_end_time - vm_recoil_start_time);
		t = glm::clamp(t, 0.f, 1.f);
		viewmodel_recoil_ofs = GetRecoilAmtTriangle(vec3(0.0, 0, 0.3), t, 0.4f);
	}break;
	}
}


void ClientGame::UpdateViewModelOffsets()
{
	PlayerState* lastp = &client.lastpredicted;
	glm::vec3 view_front = AnglesToVector(client.view_angles.x, client.view_angles.y);
	view_front.y = 0;
	glm::vec3 side_grnd = glm::normalize(glm::cross(view_front, vec3(0, 1, 0)));
	float spd_side = dot(side_grnd, lastp->velocity);
	float side_ofs_ideal = -spd_side / 200.f;
	glm::clamp(side_ofs_ideal, -0.005f, 0.005f);
	float spd_front = dot(view_front, lastp->velocity);
	float front_ofs_ideal = spd_front / 200.f;
	glm::clamp(front_ofs_ideal, -0.007f, 0.007f);
	float up_spd = lastp->velocity.y;
	float up_ofs_ideal = -up_spd / 200.f;
	glm::clamp(up_ofs_ideal, -0.007f, 0.007f);

	if (lastp->ducking)
		up_ofs_ideal += 0.04;

	viewmodel_offsets = damp(viewmodel_offsets, vec3(side_ofs_ideal, up_ofs_ideal, front_ofs_ideal), 0.01f, core.frame_time*100.f);

	//viewmodel_offsets = glm::mix(viewmodel_offsets, vec3(side_ofs_ideal, up_ofs_ideal, front_ofs_ideal), 0.4f);
}