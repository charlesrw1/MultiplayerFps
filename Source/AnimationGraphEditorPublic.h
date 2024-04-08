#pragma once
#include <SDL2/SDL.h>
#include "DrawPublic.h"
class AnimationGraphEditorPublic
{
public:
	virtual void init() = 0;
	virtual void open(const char* name) = 0;
	virtual void close() = 0;
	virtual void tick(float dt) = 0;
	virtual const View_Setup& get_vs() = 0;
	virtual void overlay_draw() = 0;
	virtual void handle_event(const SDL_Event& event) = 0;
	virtual const char* get_name() = 0;
	virtual void begin_draw() = 0;
};

extern AnimationGraphEditorPublic* g_anim_ed_graph;