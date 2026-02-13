#pragma once
#include <glm/glm.hpp>
#include <unordered_map>
#include "Game/Entity.h"
/*
api

if(ImEd::box_handles(transform))
	set bounding box etc


lowest layer: just display and user control
middle layer: which object? calls lowest layer. also handles creating commands.

*/

// immediate mode editor handle drawing
