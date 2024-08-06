#pragma once
#include "UI/GUIPublic.h"
CLASS_H(GUIButton, GUI)
public:
	MulticastDelegate<> on_selected;
};