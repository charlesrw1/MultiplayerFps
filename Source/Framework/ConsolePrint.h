#pragma once
#include "Framework/Util.h"

enum class ConColor : uint8_t
{
	Black,
	Red,
	Green,
	Yellow,
	Blue,
	Magenta,
	Cyan,
	White,
};
struct ConTextColor
{
	ConColor fg= ConColor::White;
	ConColor bg=ConColor::Black;
	bool strong_fg=false;
	bool string_bg=false;
};
