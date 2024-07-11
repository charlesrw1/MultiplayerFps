#include "Base_node.h"
#include "Basic_nodes.h"
#include "Blendspace_nodes.h"
#include "Root_node.h"
#include "State_node.h"
#include "Statemachine_node.h"
#include "Framework/AddClassToFactory.h"
#include "Framework/ReflectionRegisterDefines.h"


CLASS_IMPL(Base_EdNode);

CLASS_IMPL(Clip_EdNode);
CLASS_IMPL(Additive_EdNode);
CLASS_IMPL(Subtract_EdNode);
CLASS_IMPL(Root_EdNode);
CLASS_IMPL(Statemachine_EdNode);
CLASS_IMPL(Blend_EdNode);
CLASS_IMPL(Blend_int_EdNode);

CLASS_IMPL(Mirror_EdNode);
CLASS_IMPL(Blendspace2d_EdNode);
CLASS_IMPL(Blend_Layered_EdNode);

CLASS_IMPL(State_EdNode);
CLASS_IMPL(StateStart_EdNode);
CLASS_IMPL(StateAlias_EdNode);

CLASS_IMPL(MeshToLocalspace_EdNode);
CLASS_IMPL(LocalToMeshspace_EdNode);
CLASS_IMPL(GetCachedPose_EdNode);
CLASS_IMPL(SavePoseToCache_EdNode);
CLASS_IMPL(DirectPlaySlot_EdNode);
CLASS_IMPL(TwoBoneIK_EdNode);
CLASS_IMPL(CopyBone_EdNode);

CLASS_IMPL(FloatConstant_EdNode);
CLASS_IMPL(Curve_EdNode);
CLASS_IMPL(VectorConstant_EdNode);
CLASS_IMPL(Variable_EdNode);
CLASS_IMPL(RotationConstant_EdNode);
CLASS_IMPL(ModifyBone_EdNode);



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
