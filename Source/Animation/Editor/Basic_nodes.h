#pragma once
#include "AnimationGraphEditor.h"
#include "Base_node.h"

#include "../Runtime/AnimationTreeLocal.h"
#include "Animation/Editor/AnimationGraphEditor.h"



template<typename T>
inline bool util_create_or_ensure(T*& node)
{
	if (node) {
		ASSERT(node->get_type() == T::StaticType);
		return false;
	}
	else {
		anim_graph_ed.util_create_node(node);
		return true;
	}
}

inline void util_default_init(Base_EdNode* ednode, BaseAGNode* node, bool is_first)
{
	// init nodes by inspecting reflected node properties
	ednode->init_graph_nodes_from_node();

	// find corresponding node in runtime graph
	for (int i = 0; i < ednode->inputs.size(); i++) {
		if (!ednode->inputs[i].prop_link)
			continue;

		BaseAGNode** node_prop = (BaseAGNode**)ednode->inputs[i].prop_link->get_ptr(node);

		if (!*node_prop)
			continue;
		
		Base_EdNode* ed_node = anim_graph_ed.editor_node_for_cfg_node(*node_prop);

		if (!ed_node) {
			printf("!!! couldn't find editor node for cfg !!! (data read wrong from disk or out of date?)\n");

			ASSERT(0);
			// TODO: create the new editor node
		}

		ednode->inputs[i].node = ed_node;
	}
}


inline bool util_compile_default(Base_EdNode* node, const AgSerializeContext* ctx)
{
	auto cfg = node->get_graph_node();

	auto props = cfg->get_type().props;

	int input_index = 0;
	bool missing_inputs = false;
	for (int i = 0; i < node->inputs.size(); i++) {
		if (!node->inputs[i].prop_link)
			continue;
		auto& input = node->inputs[i];
		auto& prop = *input.prop_link;

		Base_EdNode* other_ed_node = input.node;

		BaseAGNode* other = (other_ed_node) ? other_ed_node->get_graph_node() : nullptr;

		BaseAGNode** ptr_to_ptr = (BaseAGNode**)prop.get_ptr(cfg);
		if (other_ed_node && !(other_ed_node->can_output_to_type(input.type))) {
			node->append_info_msg("[INFO] node input is wrong type, removing it (this should have not errored)\n");
			*ptr_to_ptr = ptr_to_serialized_nodecfg_ptr(nullptr, ctx);
			
			node->on_remove_pin(i);
			node->on_post_remove_pins();

			other = nullptr;	// doesnt  have input anymore
		}
		else {
			*ptr_to_ptr = ptr_to_serialized_nodecfg_ptr(other, ctx);
		}

		if (!other && input.is_node_required)
			missing_inputs = true;

		input_index++;
	}
	if (missing_inputs)
		node->append_fail_msg("[ERROR] node is missing inputs");

	return node->has_errors();
}

#define MAKE_STANDARD_FUNCTIONS(name, color, desc) \
	std::string get_name() const override { return name;} \
	Color32 get_node_color() const override { return color; } \
	std::string get_tooltip() const override { return desc; }

#define MAKE_OUTPUT_TYPE(pin_type) \
	virtual GraphPinType get_output_pin_type() const override { return GraphPinType(GraphPinType::pin_type); }

#define MAKE_STANARD_SERIALIZE(type_name) \
	static const PropertyInfoList* get_props() { \
		START_PROPS(type_name) \
			REG_STRUCT_CUSTOM_TYPE(node, PROP_SERIALIZE, "SerializeNodeCFGRef") \
		END_PROPS(type_name) \
	}


template<typename T>
class BaseNodeUtil_EdNode : public Base_EdNode
{
public:
	bool compile_my_data(const AgSerializeContext* ctx) override {
		return util_compile_default(this, ctx);
	}
	void init() override {
		util_create_or_ensure<T>(node);
		util_default_init(this, node, is_this_node_created());
		clear_newly_created();
	}
	BaseAGNode* get_graph_node() override { return node; }

	GraphPinType get_output_type_general() const override {
		return GraphPinType(GraphPinType::localspace_pose);
	}

	T* node = nullptr;
};


CLASS_H_EXPLICIT_SUPER(Variable_EdNode, BaseNodeUtil_EdNode<VariableNode>, Base_EdNode)
MAKE_STANDARD_FUNCTIONS(
	"Variable",
	VALUE_COLOR,
	"References a C++ variable"
);

GraphPinType get_output_type_general() const override {
	return GraphPinType(variable.type);
}

static const PropertyInfoList* get_props() {

	START_PROPS(Variable_EdNode)
		REG_STRUCT_CUSTOM_TYPE(node, PROP_SERIALIZE, "SerializeNodeCFGRef"),
		// hack moment
		REG_STRUCT_CUSTOM_TYPE(variable, PROP_EDITABLE, "FindAnimGraphVariableProp"),
		REG_STDSTRING(variable.str, PROP_SERIALIZE, ""),
		REG_ENUM(variable.type, PROP_SERIALIZE, "", anim_graph_value)
	END_PROPS(Variable_EdNode)
}
std::string get_title() const override {
	if (variable.str.empty()) return get_name();
	return variable.str;
}
bool compile_my_data(const AgSerializeContext* ctx) override {

	anim_graph_value found_type{};
	node->var_name = variable.str;
	auto pi = ctx->tree->find_animator_instance_variable(variable.str);

	if (!pi)
		append_fail_msg("[ERROR] node variable handle is invalid");
	else {
		bool good = false;
	
		found_type = core_type_id_to_anim_graph_value(&good, pi->type);
		if (found_type == anim_graph_value::bool_t) found_type == anim_graph_value::float_t;
			if (found_type != variable.type) {
				append_info_msg("[INFO] variable found type differs from current type, overriding it");
				sys_print(Warning, "After compiling, variable %s's type differs from previous type stored, overriding it...\n", variable.str.c_str());
				variable.type = found_type;
			}
	}
	return util_compile_default(this, ctx);
}

VariableNameAndType variable;

};

CLASS_H_EXPLICIT_SUPER(FloatConstant_EdNode, BaseNodeUtil_EdNode<FloatConstant>, Base_EdNode)
MAKE_STANDARD_FUNCTIONS(
	"Float Constant",
	VALUE_COLOR,
	"placeholder",
	);
MAKE_STANARD_SERIALIZE(FloatConstant_EdNode)

GraphPinType get_output_type_general() const override {
	return GraphPinType(anim_graph_value::float_t);
}

};

CLASS_H_EXPLICIT_SUPER(Curve_EdNode, BaseNodeUtil_EdNode<CurveNode>, Base_EdNode)
MAKE_STANDARD_FUNCTIONS(
	"Curve",
	VALUE_COLOR,
	"placeholder",
	);
MAKE_STANARD_SERIALIZE(Curve_EdNode);

GraphPinType get_output_type_general() const override {
	return GraphPinType(anim_graph_value::float_t);
}

};

CLASS_H_EXPLICIT_SUPER(VectorConstant_EdNode, BaseNodeUtil_EdNode<VectorConstant>, Base_EdNode)
MAKE_STANDARD_FUNCTIONS(
	"Vector Constant",
	VALUE_COLOR,
	"placeholder",
	);
MAKE_STANARD_SERIALIZE(VectorConstant_EdNode)

GraphPinType get_output_type_general() const override {
	return GraphPinType(anim_graph_value::vec3_t);
}

};
CLASS_H_EXPLICIT_SUPER(RotationConstant_EdNode, BaseNodeUtil_EdNode<RotationConstant>, Base_EdNode)
MAKE_STANDARD_FUNCTIONS(
	"Rotation Constant",
	VALUE_COLOR,
	"placeholder",
	);
MAKE_STANARD_SERIALIZE(RotationConstant_EdNode);

GraphPinType get_output_type_general() const override {
	return GraphPinType(anim_graph_value::quat_t);
}

};



CLASS_H_EXPLICIT_SUPER(Clip_EdNode, BaseNodeUtil_EdNode<Clip_Node_CFG>, Base_EdNode)

	MAKE_STANDARD_FUNCTIONS(
		"Clip", 
		SOURCE_COLOR, 
		"Plays an animation", 
	);
	MAKE_STANARD_SERIALIZE(Clip_EdNode);
	
	bool compile_my_data(const AgSerializeContext* ctx) override {
		if (!node->Clip.ptr||!node->Clip.ptr->seq)
			append_fail_msg("[ERROR] clip name is empty\n");

		return has_errors();
	}
	
	std::string get_title() const override {
		if (!node->Clip.ptr) return get_name();
		return node->Clip->get_name();
	}
};

CLASS_H_EXPLICIT_SUPER(Frame_Evaluate_EdNode, BaseNodeUtil_EdNode<Frame_Evaluate_CFG>, Base_EdNode)

MAKE_STANDARD_FUNCTIONS(
	"Frame evaluator",
	CACHE_COLOR,
	"Returns a single frame of an animation clip",
	);
MAKE_STANARD_SERIALIZE(Frame_Evaluate_EdNode);

bool compile_my_data(const AgSerializeContext* ctx) override {
	if (node->clip_name.empty())
		append_fail_msg("[ERROR] clip name is empty\n");

	return util_compile_default(this, ctx);
}

std::string get_title() const override {
	if (node->clip_name.empty()) return get_name();
	return "Evaluate: " + node->clip_name;
}
};



CLASS_H_EXPLICIT_SUPER(Blend_EdNode, BaseNodeUtil_EdNode<Blend_Node_CFG>, Base_EdNode)

	MAKE_STANDARD_FUNCTIONS(
		"Blend",
		BLEND_COLOR,
		"Blends 2 clips according to a float value",
	);
	MAKE_STANARD_SERIALIZE(Blend_EdNode);
};

CLASS_H_EXPLICIT_SUPER(ModifyBone_EdNode, BaseNodeUtil_EdNode<ModifyBone_CFG>, Base_EdNode)
MAKE_STANDARD_FUNCTIONS(
	"Modify bone",
	IK_COLOR,
	"Sets the transform of a single bone manually",
	);
MAKE_STANARD_SERIALIZE(ModifyBone_EdNode);
GraphPinType get_output_type_general() const override {
	return GraphPinType(GraphPinType::meshspace_pose);
}
};

CLASS_H_EXPLICIT_SUPER(TwoBoneIK_EdNode, BaseNodeUtil_EdNode<TwoBoneIK_CFG>, Base_EdNode)
MAKE_STANDARD_FUNCTIONS(
	"2 Bone IK",
	IK_COLOR,
	"Peforms a 2 bone ik.\nCan set target in meshspace or in bonespace relative to another bone",
	);
MAKE_STANARD_SERIALIZE(TwoBoneIK_EdNode);
GraphPinType get_output_type_general() const override {
	return GraphPinType(GraphPinType::meshspace_pose);
}
};

CLASS_H_EXPLICIT_SUPER(CopyBone_EdNode, BaseNodeUtil_EdNode<CopyBone_CFG>, Base_EdNode)
MAKE_STANDARD_FUNCTIONS(
	"Copy Bone",
	IK_COLOR,
	"Copies the transform of one bone to another.\n",
	);
MAKE_STANARD_SERIALIZE(CopyBone_EdNode);
GraphPinType get_output_type_general() const override {
	return GraphPinType(GraphPinType::meshspace_pose);
}
};



CLASS_H_EXPLICIT_SUPER(LocalToMeshspace_EdNode, BaseNodeUtil_EdNode<LocalToMeshspace_CFG>, Base_EdNode)
MAKE_STANDARD_FUNCTIONS(
	"Local to Mesh space",
	IK_COLOR,
	"Converts current pose from local space to mesh space",
	);
MAKE_STANARD_SERIALIZE(LocalToMeshspace_EdNode);
GraphPinType get_output_type_general() const override {
	return GraphPinType(GraphPinType::meshspace_pose);
}
};

CLASS_H_EXPLICIT_SUPER(MeshToLocalspace_EdNode, BaseNodeUtil_EdNode<MeshspaceToLocal_CFG>, Base_EdNode)
MAKE_STANDARD_FUNCTIONS(
	"Mesh to Local space",
	IK_COLOR,
	"Converts current pose from mesh space to local space",
	);
MAKE_STANARD_SERIALIZE(MeshToLocalspace_EdNode);
GraphPinType get_output_type_general() const override {
	return GraphPinType(GraphPinType::localspace_pose);
}
};

CLASS_H_EXPLICIT_SUPER(SavePoseToCache_EdNode, BaseNodeUtil_EdNode<SavePoseToCache_CFG>, Base_EdNode)
MAKE_STANDARD_FUNCTIONS(
	"Save cached pose",
	CACHE_COLOR,
	"Saves a pose that can be used in another part of the graph",
	);
MAKE_STANARD_SERIALIZE(SavePoseToCache_EdNode);

bool has_output_pin() const override { return false; }

bool can_output_to_type(GraphPinType input_pin)const override {
	return false;	// no oututs pins
}
GraphPinType get_output_type_general() const override {
	return GraphPinType(GraphPinType::localspace_pose);
}
};

CLASS_H_EXPLICIT_SUPER(GetCachedPose_EdNode, BaseNodeUtil_EdNode<GetCachedPose_CFG>, Base_EdNode)
MAKE_STANDARD_FUNCTIONS(
	"Get cached pose",
	CACHE_COLOR,
	"Get a pose that references another tree of nodes, see 'Save cached pose'",
	);
MAKE_STANARD_SERIALIZE(GetCachedPose_EdNode);
};

CLASS_H_EXPLICIT_SUPER(DirectPlaySlot_EdNode, BaseNodeUtil_EdNode<DirectPlaySlot_CFG>, Base_EdNode)
MAKE_STANDARD_FUNCTIONS(
	"Direct play slot",
	DIRPLAY_COLOR,
	"Named slot that animations can be manually played to from game code",
	);
static const PropertyInfoList* get_props() {

	START_PROPS(DirectPlaySlot_EdNode)
		REG_STRUCT_CUSTOM_TYPE(node, PROP_SERIALIZE, "SerializeNodeCFGRef"),
		REG_STDSTRING(slot_name, PROP_DEFAULT)
	END_PROPS(DirectPlaySlot_EdNode)
}
std::string get_title() const override {
	if (slot_name.empty()) return get_name();
	return "Slot: " + slot_name;
}
bool compile_my_data(const AgSerializeContext* ctx) override {

	if (slot_name.empty())
		append_fail_msg("[ERROR] slot_name is empty, set slot_name to the string that assets reference to play directly");
	else {
		bool found = false;
		auto tree = anim_graph_ed.editing_tree;
		for (int i = 0; i < tree->direct_slot_names.size(); i++) {
			if (tree->direct_slot_names[i] == slot_name) {
				node->slot_index = i;
				found = true;
			}
		}
		if (!found) {
			tree->direct_slot_names.push_back(slot_name);
			node->slot_index = tree->direct_slot_names.size() - 1;

			sys_print(Debug, "compilied slot name: %s\n", slot_name.c_str());
		}
	}
	return util_compile_default(this, ctx);
}

std::string slot_name;

};


CLASS_H_EXPLICIT_SUPER(Blend_int_EdNode, BaseNodeUtil_EdNode<Blend_Int_Node_CFG>, Base_EdNode)

	MAKE_STANDARD_FUNCTIONS(
		"Blend By Int",
		BLEND_COLOR,
		"placeholder",
		);
	MAKE_STANARD_SERIALIZE(Blend_int_EdNode);

	void init() override {
		util_create_or_ensure(node);
		// creates int parameter
		util_default_init(this, node, is_this_node_created());
		// TODO: create array params

		clear_newly_created();
	}

	void draw_node_topbar() override {

		{

			ImGui::PushStyleColor(ImGuiCol_Button, Color32{ 0xff,0xff,0xff,50 }.to_uint() );
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Color32{ 0xff,0xff,0xff,128 }.to_uint() );
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, Color32{ 0xff,0xff,0xff,50 }.to_uint() );


			if (ImGui::SmallButton("Add") && num_int_inputs < MAX_INPUTS) {
				num_int_inputs += 1;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Remove") && num_int_inputs > 0) {
				on_remove_pin(num_int_inputs - 1, true);
				on_post_remove_pins();
				num_int_inputs -= 1;
			}

			ImGui::PopStyleColor(3);
		}
	}

	bool compile_my_data(const AgSerializeContext* ctx) override {
		// FIXME:
		return util_compile_default(this, ctx);

	}

	int num_int_inputs = 0;
};

CLASS_H_EXPLICIT_SUPER(Additive_EdNode, BaseNodeUtil_EdNode<Add_Node_CFG>, Base_EdNode)

	MAKE_STANARD_SERIALIZE(Additive_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Additive",
		ADD_COLOR,
		"Apply an additive clip onto the base clip",
	);
};

CLASS_H_EXPLICIT_SUPER(Subtract_EdNode, BaseNodeUtil_EdNode<Subtract_Node_CFG>, Base_EdNode)

	MAKE_STANARD_SERIALIZE(Subtract_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Subtract",
		ADD_COLOR,
		"placeholder",
	);
};

CLASS_H_EXPLICIT_SUPER(Mirror_EdNode, BaseNodeUtil_EdNode<Mirror_Node_CFG>, Base_EdNode)

	MAKE_STANARD_SERIALIZE(Mirror_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Mirror",
		MISC_COLOR,
		"Mirror the bones across an axis, bone mirroing table must be set up in model asset",
	);


};
CLASS_H_EXPLICIT_SUPER(Blend_Layered_EdNode, BaseNodeUtil_EdNode<Blend_Masked_CFG>, Base_EdNode)

	MAKE_STANDARD_FUNCTIONS(
		"Blend Layered",
		BLEND_COLOR,
		"Layer a pose on top of a base pose that can be masked",
	);

	void draw_node_topbar() override {
		ImGui::TextColored(ImVec4(0.7, 0.7, 0.7, 1.0), maskname.c_str());
	}

	//  custom compile
	bool compile_my_data(const AgSerializeContext* ctx) override {
		node->maskname = StringName(maskname.c_str());
		return util_compile_default(this, ctx);
	}

	// custom serialize
	static const PropertyInfoList* get_props() {
		START_PROPS(Blend_Layered_EdNode)
			REG_STDSTRING(maskname,PROP_DEFAULT),
			REG_STRUCT_CUSTOM_TYPE(node, PROP_SERIALIZE, "SerializeNodeCFGRef"),
		END_PROPS(Blend_Layered_EdNode)
	}

	std::string maskname = "";
};
