#pragma once
// Per-frame debug overlay for the navmesh + agents. Toggled via nav.debug.* cvars.
//
// @docs [[navigation#debug-draw]]

class NavDebugDraw
{
public:
	// Called once per game-update tick from Level::update_level. Emits Debug::add_* shapes for the
	// frame; no allocations when every cvar is off (the function returns early).
	static void tick();
};
