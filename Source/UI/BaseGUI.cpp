#pragma once

#include "Game/Entity.h"
#include "GUISystemLocal.h"

void guiBase::get_gui_children(InlineVec<guiBase*, 16>& outvec) const
{
	// ahhhh :(
	auto ent_owner = get_owner();
	for (auto e : ent_owner->get_children()) {
		guiBase* gui = e->get_component<guiBase>();
		if(gui)
			outvec.push_back(gui);
	}
}
guiBase* guiBase::get_gui_parent() const
{
	// ahhh 2.0 :(
	auto ent_owner = get_owner();
	if (!ent_owner->get_parent()) 
		return nullptr;
	return ent_owner->get_parent()->get_component<guiBase>();
}

bool guiBase::get_is_hidden() const {

	bool res = hidden;
#ifdef  EDITOR_BUILD
	res |= get_owner()->get_hidden_in_editor();
#endif //  EDITOR_BUILD
	return res;
}

guiBase::~guiBase()
{
	guiSystemLocal.remove_reference(this);
}
void guiBase::set_focus()
{
	guiSystemLocal.set_focus_to_this(this);
}
void guiBase::release_focus() {
	guiSystemLocal.set_focus_to_this(nullptr);
}

bool guiBase::has_focus() const {
	return guiSystemLocal.key_focus == this;
}
bool guiBase::is_dragging() const {
	return guiSystemLocal.mouse_focus == this;
}
bool guiBase::is_hovering() const {
	return guiSystemLocal.hovering == this;
}
void guiBase::on_changed_transform() {
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
guiBase::guiBase() {
	set_call_init_in_editor(true);
}
void guiBase::start()
{
	const bool should_be_root = get_gui_parent() == nullptr;
	if (should_be_root) {
		guiSystemLocal.add_gui_layer(this);
	}
	ASSERT(is_a_gui_root == should_be_root);
}
glm::vec2 UIAnchorPos::get_anchor_vec(guiAnchor e)
{
	using namespace glm;
	auto get_vec = [&]() -> glm::vec2 {
		switch (e) {
		case guiAnchor::TopLeft: return vec2(0, 0);
		case guiAnchor::TopRight: return vec2(1, 0);
		case guiAnchor::BotLeft: return vec2(0, 1);
		case guiAnchor::BotRight: return vec2(1, 1);

		case guiAnchor::Top: return vec2(0.5, 0);
		case guiAnchor::Bottom: return vec2(0.5, 1);
		case guiAnchor::Left: return vec2(0, 0.5);
		case guiAnchor::Right: return vec2(1, 0.5);

		case guiAnchor::Center: return vec2(0.5, 0.5);
		}
		return vec2(0, 0);
	};
	return get_vec();
}
UIAnchorPos UIAnchorPos::anchor_from_enum(guiAnchor e)
{
	glm::vec2 v = get_anchor_vec(e);
	return UIAnchorPos::create_single(v.x, v.y);
}
