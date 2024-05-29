#pragma once

#include <string>
#include "Framework/StringName.h"

// animation events stored in clip timeline
enum class AEvent_e : uint16_t
{
	None,
	TagStringName,
	FootstepR,
	FootstepL,

	// add any other game events you want
	PlaySound,
	PlayEffect,
	ShowMesh,
	HideMesh,
	MuzzleFlash,
	Attack,
	Custom,
};
struct AnimEvent
{
	AEvent_e type = AEvent_e::None;

	StringName name;
	std::string str;

};

class AE_Footstep : public AnimEvent
{
public:
};

class AE_PlaySound : public AnimEvent
{
public:


};



// state events
enum class SEvent_e : uint8_t
{
	Entry,		// guaranteed called on entry
	Continuous,	// called every tick active
	Leave,		// guaranteed called if a state ends
};

/*
	Transition script functions:

	transition_t = self (script system quirk)
	identifier = treat as a variable name, gets turned into a #define symbol basically at compile time

	( is_tag_active transition_t identifier )
	( is_tag_entered transition_t identifier )
	( is_tag_exited transition_t identifier )
	( get_active_time transition_t )
	( get_active_percentage transtion_t )

	ex:
	( not ( is_tag_active self tMyStateTag ) )

	( and ( is_tag_entered self tJumpTag )
		( between ( get_active_percentage self ) 0.5 0.8 )
	)
*/