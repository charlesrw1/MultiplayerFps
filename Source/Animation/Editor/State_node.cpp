#include "State_node.h"
#include "Statemachine_node.h"
#include "AnimationGraphEditor.h"
#include "Root_node.h"
#include "Animation/Runtime/AnimationTreeLocal.h"

bool State_EdNode::is_start_node() const
{
	return get_type() == StateStart_EdNode::StaticType;
}
bool State_EdNode::is_alias_node() const
{
	return get_type() == StateAlias_EdNode::StaticType;

}
void State_EdNode::init()
{
	if (is_regular_state_node())
		push_empty_node();

	if (!is_this_node_created())
		return;
	// loaded nodes get a different path

	Base_EdNode* parent = ed.get_owning_node_for_layer(graph_layer);
	ASSERT(parent);
	ASSERT(parent->is_a<Statemachine_EdNode>());
	parent_statemachine = parent->cast_to<Statemachine_EdNode>();
	parent_statemachine->add_node_to_statemachine(this);

	if (is_regular_state_node()) {
		ASSERT(!sublayer.context);
		sublayer = ed.create_new_layer(false);
		ed.add_root_node_to_layer(this, sublayer.id, false);
	}
	else if (is_start_node()) {
		// num_inputs implicitly 0
	}
}
void State_EdNode::init_for_statemachine(Statemachine_EdNode* parent, std::vector<bool>& transition_taken_bitmask,
	const std::vector<State_EdNode*>& handle_to_ednode)
{
	ASSERT(!is_this_node_created());
	parent_statemachine = parent;

	if (is_regular_state_node()) {
		ASSERT(state_handle_internal.is_valid());
		self_state = *parent_statemachine->get_state(state_handle_internal);
		
		for (int i = 0; i < self_state.transition_idxs.size(); i++) {
			int index = self_state.transition_idxs[i];
			if (transition_taken_bitmask[index])
				continue;
			State_Transition& t = parent_statemachine->node->transitions.at(index);
			State_EdNode* to = handle_to_ednode[t.transition_state.id];
			ASSERT(to->is_regular_state_node());
			to->push_input(&ed, this);
			// this is horrible, add_input calls on_output on this
			ASSERT(output.back().output_to == to);
			output.back().st = t;

			transition_taken_bitmask[index] = true;
		}
	}
	else if (is_start_node()) {
		Statemachine_Node_CFG* sm = parent_statemachine->node;

		for (int i = 0; i < sm->entry_transitions.size(); i++) {
			int index = sm->entry_transitions[i];
			if (transition_taken_bitmask[index])
				continue;
			State_Transition& t = parent_statemachine->node->transitions.at(index);
			State_EdNode* to = handle_to_ednode[t.transition_state.id];
			ASSERT(to->is_regular_state_node());
			to->push_input(&ed, this);
			// this is horrible, add_input calls on_output on this
			ASSERT(output.back().output_to == to);
			output.back().st = t;

			transition_taken_bitmask[index] = true;
		}
	}
	else if (is_alias_node()) {
		// FIXME
		ASSERT(0);
	}
	else
		ASSERT(0);
}

std::string State_EdNode::get_title() const
{
	if (!name.empty())
		return name;
	else if (!is_regular_state_node())
		return get_name();

	bool any_non_defaults = false;

	auto startnode = ed.find_first_node_in_layer<Root_EdNode>(sublayer.id);

	if (!startnode->inputs[0].node || !startnode->inputs[0].node->is_a<Clip_EdNode>())
		return get_name();

	// get clip name to use as default state name
	return startnode->inputs[0].node->get_title();
}
bool State_EdNode::push_imnode_link_colors(int index) {
	ASSERT(inputs[index].node);
	State_EdNode* other =inputs[index].node->cast_to<State_EdNode>();
	ASSERT(other);

	auto st = other->get_state_transition_to(this, index);
	ASSERT(st);

	if (st->is_a_continue_transition()) {
		ImNodes::PushColorStyle(ImNodesCol_Link, Color32{ 242, 41, 41 }.to_uint());
		ImNodes::PushColorStyle(ImNodesCol_LinkSelected, Color32{ 245, 211, 211 }.to_uint());
	}
	else {
		ImNodes::PushColorStyle(ImNodesCol_Link, Color32{ 0,0xff,0 }.to_uint());
		ImNodes::PushColorStyle(ImNodesCol_LinkSelected, Color32{ 193, 247, 186 }.to_uint());
	}

	ImNodes::PushColorStyle(ImNodesCol_LinkHovered, Color32{ 0xff,0xff,0xff }.to_uint());

	return true;
}

std::string State_EdNode::get_input_pin_name(int index) const
{
	if (!inputs[index].node) return {};

	std::string name = inputs[index].node->get_title();
	if (name.size() > 16) {
		name.resize(13);
		name.append("...");
	}
	return name;
}

void State_EdNode::on_remove_pin(int slot, bool force)
{
	ASSERT(inputs[slot].node&&inputs[slot].node->is_a<State_EdNode>());
	inputs[slot].node->cast_to<State_EdNode>()->remove_output_to(this, slot);
	inputs[slot].node = nullptr;
}

void State_EdNode::remove_reference(Base_EdNode* node)
{
	if (node->is_state_node()) {
		remove_output_to((State_EdNode*)node, -1);
	}

	// node gets deleted since its in the layer
	Base_EdNode::remove_reference(node);

	if (node == parent_statemachine) {
		//parent_statemachine = nullptr;
	}
}
void State_EdNode::ensure_that_inputs_are_exposed()
{
	if (inputs.size() == MAX_INPUTS)
		return;
	if (inputs.empty()) {
		GraphNodeInput in;
		in.node = nullptr;
		in.type = GraphPinType(GraphPinType::state_t);
		inputs.push_back(in);
	}
	else if (inputs.back().node != nullptr) {
		GraphNodeInput in;
		in.node = nullptr;
		in.type = GraphPinType(GraphPinType::state_t);
		inputs.push_back(in);
	}
}
bool State_EdNode::add_input(AnimationGraphEditor* ed, Base_EdNode* input, uint32_t slot)
{
	ASSERT(input->is_state_node());
	ASSERT(inputs[slot].type.type == GraphPinType::state_t);

	inputs[slot].node = input;

	State_EdNode* statenode = input->cast_to<State_EdNode>();
	statenode->on_output_create(this, slot);

	ensure_that_inputs_are_exposed();

	return false;
}


bool State_EdNode::compile_data_for_statemachine(const AgSerializeContext* ctx)
{
	compile_error_string.clear();
	compile_info_string.clear();
	std::vector<int> sort_priority;
	Statemachine_Node_CFG* sm_cfg = parent_statemachine->node;
	if (is_regular_state_node()) {

		self_state.transition_idxs.resize(0);
	}
	for (int i = 0; i < output.size(); i++) {
		State_EdNode* out_state = output[i].output_to;
		State_Transition* st = &output[i].st;

		if (!st->is_continue_transition)
			st->automatic_transition_rule = false;

		st->transition_state = output[i].output_to->state_handle_internal;

		// compile transition script
		if (!st->conditions.empty() && st->is_a_continue_transition()) {
			append_info_msg(string_format("[INFO] is_continue_transition == true, but conditions is not empty.\n"));
		}
		else if (!st->is_a_continue_transition()) {
		}

		sm_cfg->transitions.push_back(*st);
		int idx = sm_cfg->transitions.size() - 1;
		if (is_regular_state_node()) {
			self_state.transition_idxs.push_back(idx);
		}
		else if (is_start_node()) {// entry state
			sm_cfg->entry_transitions.push_back(idx);
		}
	}

	// append tree
	if (is_regular_state_node()) {
		Root_EdNode* startnode = ed.find_first_node_in_layer<Root_EdNode>(sublayer.id);
		ASSERT(startnode);

		Node_CFG* tree = startnode->get_root_node();

		if (!tree) {
			append_fail_msg(string_format("[ERROR] missing start state in blend tree \n"));
		}

		self_state.tree = (Node_CFG*)ptr_to_serialized_nodecfg_ptr(tree, ctx);	

		sm_cfg->states.push_back(self_state);
	}

	return compile_error_string.empty();	// empty == no errors generated
}

void State_EdNode::remove_output_to(State_EdNode* node, int slot)
{
	bool already_seen = false;
	for (int i = 0; i < output.size(); i++) {

		// WARNING transitions invalidation potentially!
		if (output[i].output_to == node && (slot == -1 || slot == output[i].output_to_index)) {
			output.erase(output.begin() + i);
			already_seen = true;
			i--;
		}
	}
	ASSERT(slot == -1 || already_seen);
	// output array might become invalidated
	ed.signal_nessecary_prop_ed_reset();
}

 void State_EdNode::get_transition_props(State_EdNode* to, std::vector<PropertyListInstancePair>& props, int slot)
{
	for (int i = 0; i < output.size(); i++) {
		if (output[i].output_to == to && output[i].output_to_index == slot) {
			// WARNING: this pointer becomes invalid if output is resized
			props.push_back({ State_Transition::get_props(), &output[i].st });
			return;
		}
	}
	ASSERT(0);
}

 bool State_EdNode::traverse_and_find_errors() {
	children_have_errors = false;
	if (is_regular_state_node()) {
		auto startnode = ed.find_first_node_in_layer<Root_EdNode>(sublayer.id);

		if (startnode->get_root_node())
			children_have_errors |= !startnode->inputs[0].node->traverse_and_find_errors();
		// else is an error too, but its already built into compile_error_string
	}
	return !children_have_errors && compile_error_string.empty();
}

 void State_EdNode::on_post_remove_pins()
 {
	 ASSERT(inputs.size() >= 1);
	 ASSERT(inputs.back().node == nullptr);

	 int count = 0;
	 for (int i = 0; i < inputs.size() - 1; i++) {
		 if (inputs[i].node) {
			 State_EdNode* statenode = inputs[i].node->cast_to<State_EdNode>();
			 ASSERT(statenode);

			 statenode->reassign_output_slot(this, i, count);

			 inputs[count++] = inputs[i];
		 }
	 }
	 inputs.resize(count);
	 
	 ensure_that_inputs_are_exposed();

#ifdef _DEBUG
	 ASSERT(inputs.size() >= 1);
	 ASSERT(inputs.back().node == nullptr);
	 if (inputs.size() > 1) {
		 for (int i = 0; i < inputs.size() - 1; i++) {
			 ASSERT(inputs[i].node != nullptr);
		 }
	 }
#endif // _DEBUG
 }

#include "Framework/AddClassToFactory.h"
#include "AnimationGraphEditor.h"

 class StateTransitionPropertyEditor : public IPropertyEditor
 {
	 bool internal_update() override {
		 StateTransitionScript* self = (StateTransitionScript*)instance;
		 ImGuiTableFlags const flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame;

		 if(ImGui::BeginTable("mytable", 3, flags)) {

			 ImGui::TableNextRow();
			 ImGui::TableNextColumn();

			 draw_value_header_side(self, false);

			 ImGui::TableNextColumn();

			 const char* drop_down_opt[] = { "==","!=","<","<=",">",">=" };
			 auto preview_value = drop_down_opt[(int)self->comparison];
			 if (ImGui::BeginCombo("##optmenu", preview_value)) {
				 for (int i = 0; i < 6; i++) {
					 bool selected = (i == (int)self->comparison);
					 if (ImGui::Selectable(drop_down_opt[i], &selected)) {
						 self->comparison = (ScriptComparison)i;
					 }
				 }
				 ImGui::EndCombo();
			 }

			 ImGui::TableNextColumn();

			 draw_value_header_side(self, true);

			 ImGui::TableNextRow();
			 ImGui::TableNextColumn();
			 draw_value_side(self, false);
			 ImGui::TableNextColumn();
			 ImGui::TableNextColumn();
			 draw_value_side(self, true);

			 ImGui::EndTable();
		 }

		 return false;
	 }
	 void draw_value_header_side(StateTransitionScript* self, bool rhs) {
		 const char* opt[] = { "<none>","Variable","Constant" };
		 auto& vd = (rhs)? self->rhs:self->lhs;
		 if ((int)vd.type > 2 || (int)vd.type<0) vd.type = ScriptValueType::None;
		 auto preview = opt[(int)vd.type];
		 auto id = (rhs) ? "##rhs" : "##lhs";

		 if (ImGui::BeginCombo(id, preview)) {
			 for (int i = 0; i < 3; i++) {
				 bool selected = (int)vd.type == i;
				 if (ImGui::Selectable(opt[i], &selected)) {
					 vd.type = ScriptValueType(i);
					 vd.str = "";
				 }
			 }
			 ImGui::EndCombo();
		 }
	 }
	 void draw_value_side(StateTransitionScript* self, bool rhs) {
		 auto& vd = (rhs) ? self->rhs : self->lhs;
		 auto id = (rhs) ? "##rhs" : "##lhs";
		 ImGui::PushID(id);

		 if (vd.type == ScriptValueType::Constant) {
			 float f = 0.0;
			 int ret = sscanf(vd.str.c_str(), "%f", &f);
			 if (ret == 0) vd.str = "0";
			 if (ImGui::InputFloat("##value", &f)) {
				 vd.str = string_format("%f", f);
			}
		 }
		 else if (vd.type == ScriptValueType::Variable) {

			 const AnimatorInstance* a = ed.out.get_animator();
			 if (!a) {
				 ImGui::PopID();
				 return;
			 }

			 std::vector<const PropertyInfoList*> getprop;
			 const ClassTypeInfo* type = &a->get_type();
			 for (; type; type = type->super_typeinfo)
				 getprop.push_back(type->props);
			 auto preview = vd.str.empty() ? "<none>" : vd.str;
			 if (ImGui::BeginCombo("##varlist", preview.c_str())) {
				 for (int i = 0; i < getprop.size(); i++) {
					 auto list = getprop[i];
					 for (int j = 0; list && j < list->count; j++) {

						 bool good = false;
						 auto type = core_type_id_to_anim_graph_value(&good, list->list[j].type);
						 if (!good)
							 continue;
						 if (type != anim_graph_value::float_t && type != anim_graph_value::bool_t && type != anim_graph_value::int_t)
							 continue;
						 auto& prop = list->list[j];
						 bool selected = prop.name == vd.str;
						 if (ImGui::Selectable(prop.name, &selected)) {
							 vd.str = prop.name;
						 }

					 }
				 }
				 ImGui::EndCombo();
			 }
		 }


		 ImGui::PopID();
	 }
 };
 ADDTOFACTORYMACRO_NAME(StateTransitionPropertyEditor, IPropertyEditor, "StateTransitionScript");
