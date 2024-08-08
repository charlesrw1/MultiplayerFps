#include "SoundLocal.h"

CLASS_IMPL(SoundFile);

static SoundSystemLocal soundsys_local;
SoundSystemPublic* isound = &soundsys_local;

ConfigVar snd_max_voices("snd.max_voices", "16", CVAR_INTEGER | CVAR_DEV, 0, 64);