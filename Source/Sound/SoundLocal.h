#include "SoundPublic.h"
#include <SDL2/SDL_mixer.h>
#include "Framework/Util.h"
#include "Framework/FreeList.h"
#include "IAsset.h"
#include <glm/glm.hpp>
CLASS_H(SoundFile,IAsset)
public:
};


class SoundLoader
{
public:
};

class SoundComponent
{

};

class NavMeshComponent
{

};

class NavAgentComponent
{

};

class TimelineComponent
{

};

struct SoundPlayer
{
    const SoundFile* asset = nullptr;
    glm::vec3 spatial_pos{};
};

class SoundSystemLocal
{
public:
	void init() {
        //Initialize SDL_mixer
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
            Fatalf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
        }

	}
    void stop_all_sounds() {
    
    }



	Free_List<SoundPlayer> all_sound_players;
};