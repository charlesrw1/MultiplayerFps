#pragma once

#include "Game/Entity.h"
#include "GUISystemLocal.h"
namespace gui {

void BaseGUI::get_gui_children(InlineVec<BaseGUI*, 16>& outvec) const
{
	// ahhhh :(
	auto ent_owner = get_owner();
	for (auto e : ent_owner->get_children()) {
		BaseGUI* gui = e->get_component<BaseGUI>();
		if(gui)
			outvec.push_back(gui);
	}
}
BaseGUI* BaseGUI::get_gui_parent() const
{
	// ahhh 2.0 :(
	auto ent_owner = get_owner();
	if (!ent_owner->get_parent()) 
		return nullptr;
	return ent_owner->get_parent()->get_component<BaseGUI>();
}

bool BaseGUI::get_is_hidden() const {

	bool res = hidden;
#ifdef  EDITOR_BUILD
	res |= get_owner()->get_hidden_in_editor();
#endif //  EDITOR_BUILD
	return res;
}

BaseGUI::~BaseGUI()
{
	guiSystemLocal.remove_reference(this);
}
void BaseGUI::set_focus()
{
	guiSystemLocal.set_focus_to_this(this);
}
void BaseGUI::release_focus() {
	guiSystemLocal.set_focus_to_this(nullptr);
}

bool BaseGUI::has_focus() const {
	return guiSystemLocal.key_focus == this;
}
bool BaseGUI::is_dragging() const {
	return guiSystemLocal.mouse_focus == this;
}
bool BaseGUI::is_hovering() const {
	return guiSystemLocal.hovering == this;
}
void BaseGUI::on_changed_transform() {
	const bool should_be_root = get_gui_parent() == nullptr;
	if (is_a_gui_root!= should_be_root) {
		if (should_be_root) {
			guiSystemLocal.add_gui_layer(this);
		}
		else {
			guiSystemLocal.remove_gui_layer(this);
		}
		ASSERT(is_a_gui_root == should_be_root);
	}
}
BaseGUI::BaseGUI() {
	set_call_init_in_editor(true);
}
void BaseGUI::start()
{
	const bool should_be_root = get_gui_parent() == nullptr;
	if (should_be_root) {
		guiSystemLocal.add_gui_layer(this);
	}
	ASSERT(is_a_gui_root == should_be_root);
}

}