#pragma once

#include "Framework/ClassBase.h"
#include "Game/Entity.h"
#include "Framework/LuaColor.h"
struct HitResult
{
	STRUCT_BODY();
	REF obj<Entity> what;
	REF glm::vec3 pos;
	REF glm::vec3 normal;
	REF bool hit = false;
};




using std::vector;
class PhysicsBody;
#include "Game/Components/PhysicsComponents.h"
#include "../Level.h"
#include "Sound/SoundPublic.h"
class SpawnerComponent;
class GameplayStatic : public ClassBase
{
public:
	CLASS_BODY(GameplayStatic);

	REFLECT(no_nil)
	static Entity* spawn_entity();

	// spatialize=R/L ear panning
	// attenuation=distance attenuation
	REF static void play_spatial_sound_ex(glm::vec3 pos, SoundFile* sound, float min_rad, float max_rad,
		SndAtn attenuation, bool attenuate, bool spatialize) {
		isound->play_sound(sound, 1, 1, min_rad, max_rad, attenuation, attenuate, spatialize, pos);
	}
	REF static void play_spatial_sound(glm::vec3 pos, SoundFile* sound, float min_rad, float max_rad,
		SndAtn attenuation) {
		play_spatial_sound_ex(pos, sound, min_rad, max_rad, attenuation, true, true);
	}
	REF static void play_simple_sound(SoundFile* sound) { isound->play_sound(sound, 1, 1, 0, 0, {}, false, false, {}); }

	REF static std::vector<SpawnerComponent*> find_spawners_in_class(std::string name);

	REF static HitResult cast_ray(glm::vec3 start, glm::vec3 end, int channel_mask, PhysicsBody* ignore_this);

	REF static int get_collision_mask_for_physics_layer(PL physics_layer);

	REF static void send_back_result(HitResult res) { printf("%f %f %f\n", res.pos.x, res.pos.y, res.pos.z); }

	REF static vector<Component*> find_components(const ClassTypeInfo* info);
	REF static Entity* find_by_name(string name);
	REF static float get_dt() { return eng->get_dt(); }
	REF static float get_time() { return eng->get_game_time(); }

	REF static bool change_level(string mapname) { return eng->load_level(mapname); }

	REF static string get_current_level_name() {
		if (!eng->get_level())
			return "";
		return eng->get_level()->get_source_asset_name();
	}
	REF static std::vector<obj<Entity>> sphere_overlap(glm::vec3 center, float radius, int channel_mask);

	REF static void reset_debug_text_height();
	REF static void debug_text(string s);
	REF static void debug_sphere(glm::vec3 center, float radius, float life, const lColor& color);
	REF static void debug_line_normal(glm::vec3 p, glm::vec3 n, float len, float life, const lColor& color);

	// kind of hack bs till i work it out better
	// basically nil tables are null and can be checked, but when an object is deleted, the _ptr field int he table is
	// nullptr'd, but the table is non-nil i dont think you can check _ptr in lua since its userdata. so this will get
	// the ClassBase* which does the nil and _ptr null check etc.
	REF static bool is_null(ClassBase* e) { return e == nullptr; }

	REF static void enable_ragdoll_shared(Entity* e, bool enable);

	REF static void debug_break() { __debugbreak(); }
	REF static bool is_editor() { return eng->is_editor_level(); }




};