#include "ClipNode.h"
#include "AnimationGraphEditor2.h"

#include "Animation/Runtime/RuntimeNodesNew.h"

template<typename T, typename... Ts>
inline T get_or(const std::variant<Ts...>& v, const T& fallback) {
	if (auto ptr = std::get_if<T>(&v)) {
		return *ptr;
	}
	return fallback;
}
#include "Animation/Runtime/RuntimeValueNodes.h"

opt<int> create_linked_node(CompilationContext& ctx, int input_index, Base_EdNode* node)
{
	auto port = node->find_my_port(input_index, false);
	if (!port) {
		ctx.add_error(node->self, "Couldn't find port index");
		return std::nullopt;
	}

	using std::get;
	opt<GraphLink> link = node->find_link_from_port(port->get_handle(node->self));
	if (!link.has_value()) {
		opt<variant<bool, int, float, vec3>> values;
		switch (port->type.type) {
		case GraphPinType::Float: {
			values = get_or<float>(port->inlineValue, 0.f);
		}break;
		case GraphPinType::EnumType:
		case GraphPinType::Integer: {
			values = get_or<int>(port->inlineValue, 0);
		}break;
		case GraphPinType::Boolean: {
			values = get_or<bool>(port->inlineValue, false);
		}break;
		case GraphPinType::Vec3: {
			values = get_or<vec3>(port->inlineValue, vec3(0.f));
		}break;
		default:
			break;
		}
		if (values.has_value()) {
			atConstantNode* outnode = new atConstantNode;
			outnode->values = values.value();
			return ctx.add_inline_output_node(node->self,port->get_idx(), outnode);
		}
		else {
			return std::nullopt;
		}
	}
	else {
		auto otherport = node->get_other_nodes_port(link.value());
		auto othernode = node->editor->get_graph().get_node(link->get_other_node(node->self));
		if (!otherport || !othernode) {
			ctx.add_warning(node->self, "no other port?");
			return std::nullopt;
		}
		else if (otherport->type != port->type) {
			ctx.add_error(node->self, "type mismatch on port");
			return std::nullopt;
		}
		ctx.compile_this(othernode);
		auto outhandle = otherport->get_handle(othernode->self).id;

		if (!MapUtil::contains(ctx.output_nodes, outhandle)) {
			ctx.add_error(node->self, "node not found");
		}
		return outhandle;
	}
}



class LayerOwnerUtil
{
public:
	static NodeGraphLayer& check_or_create_sublayer(GraphLayerHandle& handle, Base_EdNode* self, AnimationGraphEditorNew& editor, opt<string> create_this_node) {
		auto& graph = editor.get_graph();
		if (!handle.is_valid()) {
			NodeGraphLayer* layer = graph.create_layer();
			layer->set_owner_node(self->self);
			handle = layer->get_id();
			if (create_this_node.has_value()) {
				auto newnode = editor.get_prototypes().create(create_this_node.value());
				if (newnode)
					graph.insert_new_node(*newnode, handle, std::nullopt);
				else
					printf("couldnt create layer node\n");
			}

			printf("created sublayer %d\n", handle.id);
		}
		NodeGraphLayer* layer = graph.get_layer(handle);
		assert(layer && layer->get_owner_node() == self->self);
		return *layer;
	}
};

void Statemachine_EdNode::on_link_changes()  {
	Base_EdNode::on_link_changes();

	NodeGraphLayer& layer = LayerOwnerUtil::check_or_create_sublayer(sublayer, this, *editor, "EntryState");
	layer.set_layer_type(NodeGraphLayer::Statemachine);

}


void Statemachine_EdNode::set_owning_sublayer(GraphLayerHandle h) {

	this->sublayer = h;
}

void CommentNode::on_link_changes()
{
	Base_EdNode::on_link_changes();
	ImNodes::SetCommentNodeSize(self.id, ImVec2(sizex, sizey));
}
void Variable_EdNode::on_link_changes()
{
	this->name = "Variable";
	this->foundType = editor->get_params().find_value_type(variable_name);
	if (foundType.has_value()) {
		find_my_port(0, true)->type = foundType.value();
	}
	else{
		find_my_port(0, true)->type = GraphPinType::Any;
	}
	find_my_port(0, true)->name = variable_name;
}
void Variable_EdNode::compile(CompilationContext& ctx)
{
	if (!foundType.has_value()) {
		ctx.add_error(self, "Variable has no type.");
	}
	else {
		atVariableNode* out = new atVariableNode;
		out->varName = this->variable_name;
		ctx.add_output_node(self,0, out);
	}
}
#include "AnimCommands.h"
void BlendInt_EdNode::on_link_changes()
{
	Base_EdNode::on_link_changes();
	if (num_blend_cases != ports.size() - 2) {

		for (int i = 0; i < num_blend_cases; i++) {
			GraphPort* p = find_my_port(i, false);
			if (!p)
				add_in_port(i, std::to_string(i)).type = GraphPinType::LocalSpacePose;
		}
		vector<int> inps, outs;
		get_ports(inps, outs);
		vector<int> inp_indicies;
		for (int i : inps)
			inp_indicies.push_back(ports.at(i).index);
		for (int idx : inp_indicies) {
			if (idx >= num_blend_cases && idx != get_index_of_value_input()) {
				remove_port(idx, false);
			}
		}
		remove_links_without_port();
	}


#if 0
	auto set_type_to_other = [&](const int idx) -> bool {
		const auto myport = GraphPortHandle::make(self, enum_info->get_index_of_value_port(), false);
		const GraphPort* other = get_other_nodes_port_from_myport(myport);
		if (other) {
			const auto othertype = other->type;
			bool is_valid = false;
			if (othertype.type==GraphPinType::EnumType) {
				is_valid = true;
			}
			if (is_valid) {
				const EnumTypeInfo* prevenumtype = enum_info->what_enum_type;
				auto myportptr = get_enum_graph_port();
				myportptr->type = other->type;
				const EnumTypeInfo* next = myportptr->type.get_enum_or_set_to_null();
				enum_info->what_enum_type = next;
				return prevenumtype != next;
			}
			else {
				GraphCommandUtil::remove_link(find_link_from_port(myport).value(), editor->get_graph());
				return false;
			}
		}
		return false;
	};

	if (enum_info.has_value()) {
		assert(get_enum_graph_port()->type.type == GraphPinType::EnumType);
		const bool changed = set_type_to_other(0);
		if (changed) {
			auto putbacks = remove_all_input_ports();
			auto enumtype = enum_info->what_enum_type;
			if (enumtype) {
				for (int i = 0; i < enumtype->str_count; i++) {
					auto& poseport = add_in_port(i, enumtype->strs[i].name);
					poseport.type = GraphPinType::LocalSpacePose;
				}
			}
			auto& valueport = add_in_port(enum_info->get_index_of_value_port(), "value");
			valueport.type = GraphPinType::EnumType;
			valueport.type.data = enumtype;
			insert_putbacks(putbacks);
			auto port = get_enum_graph_port();
			assert(port && port->type.type == GraphPinType::EnumType);
		}
	}
#endif
}
void BlendInt_EdNode::on_property_changes() {
	if (num_blend_cases < 0)
		num_blend_cases = 0;
	if (num_blend_cases >= MAX_INPUTS - 1)
		num_blend_cases = MAX_INPUTS - 1;
	on_link_changes();
}

void BlendInt_EdNode::compile(CompilationContext& ctx)
{
	atBlendByInt* out = new atBlendByInt;
	out->valudId = create_linked_node(ctx, get_index_of_value_input(), this).value_or(0);
	for (int i = 0; i < num_blend_cases; i++) {
		out->inputs.push_back(create_linked_node(ctx, i, this).value_or(0));
	}
	ctx.add_output_node(self, 0, out);
}

BlendInt_EdNode::BlendInt_EdNode() {
	add_out_port(0, "").type = GraphPinType::LocalSpacePose;
	add_in_port(0, "0").type = GraphPinType::LocalSpacePose;
	add_in_port(1, "1").type = GraphPinType::LocalSpacePose;
	add_in_port(get_index_of_value_input(), "value").type = GraphPinType::Integer;
	num_blend_cases = 2;
	assert(num_blend_cases == ports.size() - 2);
}

void LogicalOp_EdNode::on_link_changes()
{
	Base_EdNode::on_link_changes();
	if (num_inputs != ports.size() - 1) {
		for (int i = 0; i < num_inputs; i++) {
			GraphPort* p = find_my_port(i, false);
			if (!p)
				add_in_port(i, "").type = GraphPinType::Boolean;
		}
		vector<int> inps, outs;
		get_ports(inps, outs);
		vector<int> inp_indicies;
		for (int i : inps)
			inp_indicies.push_back(ports.at(i).index);
		for (int idx : inp_indicies) {
			if (idx >= num_inputs) {
				remove_port(idx, false);
			}
		}
		remove_links_without_port();
	}
}

void LogicalOp_EdNode::on_property_changes()
{
	if (num_inputs < 0)
		num_inputs = 0;
	if (num_inputs >= MAX_INPUTS)
		num_inputs = MAX_INPUTS;
	on_link_changes();
}
void LogicalOp_EdNode::compile(CompilationContext& ctx)
{
	atLogicalOpNode* out = new atLogicalOpNode;
	out->is_and = !this->is_or;
	for (int i = 0; i < num_inputs; i++) {
		opt<GraphLink> l = find_link_from_port(GraphPortHandle::make(self, i, false));
		if (l.has_value()) {
			out->nodes.push_back(create_linked_node(ctx, i, this).value_or(0));
		}
	}
	ctx.add_output_node(self, 0, out);

}
#include "Animation/Runtime/Statemachine_cfg.h"//for easing, fixme
FloatMathFuncs_EdNode::FloatMathFuncs_EdNode(Type t) : funcType(t) {
	switch (t)
	{
	case Type::ScaleBias:
		add_out_port(0, "").type = GraphPinType::Float;
		add_in_port(0, "value").type = GraphPinType::Float;
		add_in_port(1, "scale").type = GraphPinType::Float;
		add_in_port(2, "bias").type = GraphPinType::Float;
		break;
	case Type::Clamp:
		add_out_port(0, "").type = GraphPinType::Float;
		add_in_port(0, "value").type = GraphPinType::Float;
		add_in_port(1, "min").type = GraphPinType::Float;
		add_in_port(2, "max").type = GraphPinType::Float;
		break;
	case Type::InRange:
		add_out_port(0, "").type = GraphPinType::Boolean;
		add_in_port(0, "value").type = GraphPinType::Float;
		add_in_port(1, "min").type = GraphPinType::Float;
		add_in_port(2, "max").type = GraphPinType::Float;
		break;
	case Type::Abs:
		add_out_port(0, "").type = GraphPinType::Float;
		add_in_port(0, "").type = GraphPinType::Float;
		break;
	case Type::Remap: {
		add_out_port(0, "").type = GraphPinType::Float;
		add_in_port(0, "value").type = GraphPinType::Float;
		{
			auto& in_min = add_in_port(1, "in_min");
			in_min.type = GraphPinType::Float;
			in_min.inlineValue = 0.f;
		}
		{
			auto& in_max = add_in_port(2, "in_max");
			in_max.type = GraphPinType::Float;
			in_max.inlineValue = 1.f;
		}
		{
			auto& out_min = add_in_port(3, "out_min");
			out_min.type = GraphPinType::Float;
			out_min.inlineValue = 0.f;
		}
		{
			auto& out_max = add_in_port(4, "out_max");
			out_max.type = GraphPinType::Float;
			out_max.inlineValue = 1.f;
		}
		GraphPort& easing = add_in_port(5, "easing");
		easing.type = GraphPinType::EnumType;
		const EnumTypeInfo* easinginfo = &EnumTrait< Easing>::StaticEnumType;
		easing.type.data = easinginfo;
	}break;
	default:
		break;
	}
}

void FloatMathFuncs_EdNode::compile(CompilationContext& ctx)
{
}

void State_EdNode::on_link_changes()
{
	Base_EdNode::on_link_changes();

	{
		NodeGraphLayer& layer = LayerOwnerUtil::check_or_create_sublayer(state_graph, this, *editor, "ReturnPose");
		layer.set_layer_type(NodeGraphLayer::BlendTree);
	}

	// rules:
	// ensure that all links are valid
	// ensure that there is 1 input pin available
	// ensure that links have a graph subobject

	for (int i = 0; i < links.size(); i++) {
		GraphLink l = links.at(i).link;
		if (!l.self_is_input(self)) {
			continue;
		}
		const GraphPort* other = get_other_nodes_port(l);
		assert(other);
		// self is input
		const GraphPort* my = get_my_node_port(l);
		if (!my) {
			auto myport = l.get_self_port(self);
			add_in_port(myport.get_index(), "").type = GraphPinType::StateType;
		}
		assert(get_my_node_port(l) != nullptr);

		if (!links.at(i).opt_link_node.is_valid()) {
			StateTransition_EdNode* transition = new StateTransition_EdNode;
			editor->get_graph().insert_new_node(*transition, layer, {});
			links.at(i).opt_link_node = transition->self;
		}

	}
	bool found_empty = false;
	for (int i = 0; i < ports.size(); i++) {
		const GraphPort& p = ports[i];
		if (p.is_output())
			continue;
		opt<GraphLink> gl = find_link_from_port(p.get_handle(self));
		if (!gl.has_value()) {
			found_empty = true;
			break;
		}
	}
	if (!found_empty) {
		vector<int> inputs, output;
		get_ports(inputs, output);
		assert(inputs.size() <= MAX_INPUTS);
		unordered_set<int> indicies;
		for (auto i : inputs)
			indicies.insert(ports.at(i).index);
		bool found = false;
		for (int i = 0; i < MAX_INPUTS; i++) {
			if (indicies.find(i) == indicies.end()) {
				add_in_port(i, "").type = GraphPinType::StateType;
				found = true;
				break;
			}
		}
		if (!found) {
			printf("State node is full\n");
		}
	}

	//remove_links_without_port();
}

void StateTransition_EdNode::on_link_changes()
{
	Base_EdNode::on_link_changes();
	NodeGraphLayer& layer = LayerOwnerUtil::check_or_create_sublayer(transition_graph, this, *editor, "ReturnTransition");
	layer.set_layer_type(NodeGraphLayer::Transition);

}
#include "Animation/AnimationSeqAsset.h"
string Clip_EdNode::get_subtitle() const {
	if (Data.Clip.get())
		return Data.Clip.get()->get_name();
	return "";
}


void Clip_EdNode::compile(CompilationContext& ctx)
{
	assert(find_my_port(0, true));
	if (!Data.Clip.get()) {
		ctx.add_error(self, "Animation clip invalid.");
	}
	else {
		atClipNode* out = new atClipNode;
		out->data = this->Data;
		opt<int> speedId = create_linked_node(ctx, 0, this);
		out->speedId = speedId.value_or(0);
		if (!speedId.has_value()) {
			ctx.add_error(self, "Doesnt have speed input.");
		}
		ctx.add_output_node(self,0, out);
	}
}
void StateAlias_EdNode::fixup_any_extra_references(const unordered_map<int, int>& old_id_to_new_id) {
	for (auto& h : data.handles) {
		if (MapUtil::contains(old_id_to_new_id, h.handle.id)) {
			h.handle.id = old_id_to_new_id.find(h.handle.id)->second;
		}
	}
}
void StateAlias_EdNode::on_link_changes()
{
	Base_EdNode::on_link_changes();

	unordered_set<State_EdNode*> nodes;
	auto layerptr = editor->get_graph().get_layer(layer);
	assert(layerptr);
	for (int i : layerptr->get_nodes()) {
		Base_EdNode* n = editor->get_graph().get_node(GraphNodeHandle(i));
		assert(n);
		State_EdNode* state = n->cast_to<State_EdNode>();
		if (state) {
			nodes.insert(state);
		}
	}
	unordered_set<State_EdNode*> true_nodes_in_array;
	for (auto& h : data.handles) {
		if (h.flag==data.default_true) continue;
		Base_EdNode* e = editor->get_graph().get_node(h.handle);
		if (e && e->layer == layer && e->is_a<State_EdNode>()) {
			SetUtil::insert_test_exists(true_nodes_in_array,e->cast_to<State_EdNode>());
		}
	}
	data.handles.clear();
	for (auto n : nodes) {
		SAHandleWithFlag handle;
		handle.handle = n->self;
		if (SetUtil::contains(true_nodes_in_array, n))
			handle.flag = !data.default_true;
		else
			handle.flag = data.default_true;

		data.handles.push_back(handle);
	}
}

void ComposePoses_EdNode::compile(CompilationContext& ctx)
{
	atComposePoses* combine = new atComposePoses;
	combine->type = is_additive ? atComposePoses::Additive : atComposePoses::Blend;
	combine->alphaId = create_linked_node(ctx, 0, this).value_or(0);
	combine->pose0Id = create_linked_node(ctx, 1, this).value_or(0);
	combine->pose1Id = create_linked_node(ctx, 2, this).value_or(0);
	ctx.add_output_node(self, 0, combine);
}

void Not_EdNode::compile(CompilationContext& ctx)
{
	atNotNode* not_node = new atNotNode;
	not_node->input = create_linked_node(ctx, 0, this).value_or(0);
	ctx.add_output_node(self, 0, not_node);
}

void Ik2Bone_EdNode::compile(CompilationContext& ctx)
{
	atIk2Bone* out = new atIk2Bone;
	out->inputId = create_linked_node(ctx, 0, this).value_or(0);
	out->targetId = create_linked_node(ctx, 1, this).value_or(0);
	out->poleId = create_linked_node(ctx, 2, this).value_or(0);
	out->alphaId = create_linked_node(ctx, 3, this).value_or(0);
	ctx.add_output_node(self, 0, out);
}

void ModifyBone_EdNode::compile(CompilationContext& ctx)
{
	atModifyBone* out = new atModifyBone;
	out->inputId = create_linked_node(ctx, 0, this).value_or(0);
	out->translationId = create_linked_node(ctx, 1, this).value_or(0);
	out->rotationId = create_linked_node(ctx, 2, this).value_or(0);
	out->alphaId = create_linked_node(ctx, 3, this).value_or(0);
	out->rotation = this->rotation;
	out->translation = this->translation;
	ctx.add_output_node(self, 0, out);
}
