#pragma once

#include "UI/BaseGUI.h"

void update_one_child_position(guiBase* me);
void update_desired_size_from_one_child(guiBase* me);
void update_desired_size_flow(guiBase* me, int axis);
void update_child_positions_flow(guiBase* me, int axis, int start);