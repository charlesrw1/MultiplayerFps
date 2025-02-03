#pragma once
#include "Framework/Util.h"
#include "EnumDefReflection.h"

NEWENUM(LogCategory, int)
{
	Core,
	Game,
	Render,
	Anim,
	Editor,
	UI,
	Script,
	Physics,
	Input,
};
using LC = LogCategory;

void sys_catprint(LogCategory category, LogType type, const char* fmt, ...);