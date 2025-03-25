#pragma once

#include "UI/BaseGUI.h"

void update_one_child_position(gui::BaseGUI* me);
void update_desired_size_from_one_child(gui::BaseGUI* me);
void update_desired_size_flow(gui::BaseGUI* me, int axis);
void update_child_positions_flow(gui::BaseGUI* me, int axis, int start);