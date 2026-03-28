#pragma once

#include <string>

class IInputReciever
{
public:
	virtual void on_focused_tick() {}
	virtual std::string get_name() { return ""; }
};