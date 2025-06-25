#pragma once
#include "ClipNode.h"

class btComposite_EdNode : public Base_EdNode {
public:
	CLASS_BODY(btComposite_EdNode);
	btComposite_EdNode(bool is_selector) {
		add_in_port(0, "").type = GraphPinType::StateType;
		add_out_port(0, "").type = GraphPinType::StateType;
	}
	Color32 get_node_color() const { return get_color_for_category(EdNodeCategory::Function); }
};
class btParallel_EdNode : public Base_EdNode {
public:
	CLASS_BODY(btParallel_EdNode);
	btParallel_EdNode() {
		add_in_port(0, "").type = GraphPinType::StateType;
		add_out_port(0, "").type = GraphPinType::StateType;
	}
	Color32 get_node_color() const { return get_color_for_category(EdNodeCategory::Function); }
};
class btDecorator_EdNode : public Base_EdNode {
public:
	CLASS_BODY(btDecorator_EdNode);
	btDecorator_EdNode() {
		add_out_port(0, "").type = GraphPinType::StateType;
		add_in_port(0, "").type = GraphPinType::StateType;
	}
	Color32 get_node_color() const { return get_color_for_category(EdNodeCategory::Math); }
};
class btFilter_EdNode : public btDecorator_EdNode {
public:
	CLASS_BODY(btFilter_EdNode);
	btFilter_EdNode() {
		add_in_port(1, "").type = GraphPinType::Boolean;
	}
};
class btAction_EdNode : public Base_EdNode {
public:
	CLASS_BODY(btAction_EdNode);
	btAction_EdNode() {
		add_in_port(0, "").type = GraphPinType::StateType;
	}
	Color32 get_node_color() const { return get_color_for_category(EdNodeCategory::None); }
};
class btDecorator : public ClassBase
{
public:
	CLASS_BODY(btDecorator);
};
class btIsPlayerInRange : public btDecorator {
public:
	CLASS_BODY(btIsPlayerInRange);
};
class btIsInSight : public btDecorator {
public:
	CLASS_BODY(btIsInSight);
};
class btWeaponHasAmmo : public btDecorator {
public:
	CLASS_BODY(btWeaponHasAmmo);
};