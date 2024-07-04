#include "Base_node.h"
#include "Basic_nodes.h"
#include "Blendspace_nodes.h"
#include "Root_node.h"
#include "State_node.h"
#include "Statemachine_node.h"
#include "Framework/AddClassToFactory.h"
#include "Framework/ReflectionRegisterDefines.h"


ABSTRACT_CLASS_IMPL_NO_PROPS(Base_EdNode, ClassBase);


#define EDIMPL(type_name) \
CLASS_IMPL(type_name, Base_EdNode)

EDIMPL(Clip_EdNode);
EDIMPL(Additive_EdNode);
EDIMPL(Subtract_EdNode);
EDIMPL(Root_EdNode);
EDIMPL(Statemachine_EdNode);
EDIMPL(Blend_EdNode);
EDIMPL(Blend_int_EdNode);
EDIMPL(Sync_EdNode);
EDIMPL(Mirror_EdNode);
EDIMPL(Blendspace2d_EdNode);
EDIMPL(Blend_Layered_EdNode);

EDIMPL(State_EdNode);
CLASS_IMPL(StateStart_EdNode, State_EdNode);
CLASS_IMPL(StateAlias_EdNode, State_EdNode);


void Base_EdNode::remove_reference(Base_EdNode* node)
{
	for (int i = 0; i < inputs.size(); i++) {
		if (inputs[i].node == node) {
			on_remove_pin(i, true);

			on_post_remove_pins();
		}
	}
}

void Base_EdNode::post_construct(uint32_t id, uint32_t graph_layer)
{
	this->id = id;
	this->graph_layer = graph_layer;
	is_newly_created = true;
}

bool Base_EdNode::traverse_and_find_errors()
{
	children_have_errors = false;
	for (int i = 0; i < inputs.size(); i++) {
		if (inputs[i].node)
			children_have_errors |= !inputs[i].node->traverse_and_find_errors();
	}

	return !children_have_errors && compile_error_string.empty();
}
