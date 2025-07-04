#include "SoundComponent.h"

#include "Framework/ReflectionMacros.h"
#include "GameEnginePublic.h"
#include "Game/Components/MeshbuilderComponent.h"
#include "Game/Entity.h"

#ifdef EDITOR_BUILD
void SoundComponent::update_ed_mesh()
{
	editor_mesh->mb.Begin();
	if (attenuate) {
		if(minRadius>0.0001)
			editor_mesh->mb.AddLineSphere({}, minRadius, { 100,100,255 });
		editor_mesh->mb.AddLineSphere({}, maxRadius, COLOR_WHITE);
	}
	editor_mesh->mb.End();
}
#endif
void SoundComponent::update_player()
{
	ASSERT(player);
	player->asset = sound.get();
	player->minRadius = minRadius;
	player->maxRadius = maxRadius;
	player->attenuation = (SndAtn)attenuation;
	player->attenuate = attenuate;
	player->spatialize = spatialize;
	player->spatial_pos = get_ws_position();
	player->looping = looping;
}
void SoundComponent::start()
{
#ifdef EDITOR_BUILD
	if (eng->is_editor_level()) {
		editor_mesh = get_owner()->create_component<MeshBuilderComponent>();
		editor_mesh->use_background_color = true;
		editor_mesh->use_transform = true;
		update_ed_mesh();
		editor_mesh->on_changed_transform();
	}
#endif
	player = isound->register_sound_player();
	update_player();
	if (enable_on_start) {
		player->set_play(true);
	}
	set_ticking(false);
}
void SoundComponent::stop()
{
	if (player) {
		isound->remove_sound_player(player);
	}
}
void SoundComponent::update()
{

}

void SoundComponent::play_one_shot_at_pos(const glm::vec3& v) const
{
	isound->play_sound(
		sound.get(),
		1, 1, minRadius, maxRadius, (SndAtn)attenuation, attenuate, spatialize, v
	);
}

void SoundComponent::editor_on_change_property()
{
#ifdef EDITOR_BUILD
	update_ed_mesh();
	if (editor_test_sound.check_and_swap()) {
		play_one_shot();
	}
	
#endif
}
void SoundComponent::on_changed_transform()
{
	if (player) {
		player->spatial_pos = get_ws_position();
	}
}

SoundComponent::SoundComponent() {
	set_call_init_in_editor(true);
}