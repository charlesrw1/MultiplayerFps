#pragma once
#include "Framework/Reflection2.h"

// a level editor tool plugin to extend the functionality of the level editor
// examples: paint tool, grid map tool, spline tool
// subclass this to implment functionality and have it appear in menus
NEWCLASS(LEPlugin,ClassBase)
public:
	// this is called first, do initial check to see if the plugin can be entered
	virtual bool can_start() { return true; }
	// start,tick,end self explanatory
	virtual void on_start() = 0;
	virtual void on_update() = 0;
	virtual void on_end() = 0;
	// called to draw an imgui window
	virtual void imgui_draw() = 0;
};