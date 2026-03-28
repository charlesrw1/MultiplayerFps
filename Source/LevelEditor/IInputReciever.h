#pragma once

#include <string>

class EditorInputs;
class IInputReciever
{
public:
	virtual void on_focused_tick(EditorInputs& inputs) {}
	virtual std::string get_name() { return ""; }
};