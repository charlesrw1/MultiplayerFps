#pragma once
#include "Framework/StringName.h"

class Entity;
class Interaction
{
public:
	virtual bool can_interact(Entity* who) = 0;
	virtual void on_start(Entity* who) = 0;
	virtual void on_end() = 0;
	virtual StringName get_id() = 0;
	virtual const char* get_display_name() = 0;
	virtual const char* get_cant_interact_reason() { return ""; }
	virtual int get_button() = 0;
	virtual void on_hover_start() = 0;
	bool is_disabled() const { return disable; }
protected:
	bool disable = false;
};