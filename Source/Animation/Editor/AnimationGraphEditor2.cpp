#include "AnimationGraphEditor2.h"
#include "LevelEditor/PropertyEditors.h"
#include "imnodes.h"
#include "Assets/AssetDatabase.h"
#include "Render/Texture.h"
#include "Framework/Log.h"
#include "GraphUtil.h"
#include "AnimCommands.h"
#include "ClipNode.h"

using std::make_unique;
AnimationGraphEditorNew animgraphnew;
IEditorTool* g_anim_ed_graph = &animgraphnew;
extern int imgui_std_string_resize(ImGuiInputTextCallbackData* data);

static ImNodesPinShape push_and_get_pin_type(const GraphPinType::Enum& type, bool selected)
{
	selected = true;
	auto [color, shape] = GraphUtil::get_pin_for_value_type(type);
	ImNodes::PushColorStyle(ImNodesCol_Pin, color.to_uint());
	switch (shape)
	{
	case GraphUtil::PinShapeColor::Circle: return selected ? ImNodesPinShape_CircleFilled : ImNodesPinShape_Circle;
	case GraphUtil::PinShapeColor::Square: return selected ? ImNodesPinShape_QuadFilled : ImNodesPinShape_Quad;
	case GraphUtil::PinShapeColor::Triangle: return selected ? ImNodesPinShape_TriangleFilled : ImNodesPinShape_Triangle;
	default:
		break;
	}
	return {};
}

void AnimationGraphEditorNew::add_command(Command* command)
{
	cmd_manager.add_command(command);
}

void GraphTabManager::handle_link_changes()
{
	int start_atr = 0;
	int end_atr = 0;
	int link_id = 0;

	if (ImNodes::IsLinkCreated(&start_atr, &end_atr))
	{
		if (start_atr >= INPUT_START && start_atr < OUTPUT_START)
			std::swap(start_atr, end_atr);
		editor.add_command(new AddLinkCommand(editor,GraphPortHandle(end_atr), GraphPortHandle(start_atr)));
	}
	if (ImNodes::IsLinkDropped(&start_atr)) {

		const GraphPortHandle port(start_atr);
		ImGui::OpenPopup("my_select_popup");
		mouse_click_pos = GraphUtil::to_glm(ImGui::GetMousePos());
		from_drop = FromDropState{ port };
		set_keyboard_focus = true;
	}
	if (ImNodes::IsLinkDestroyed(&link_id)) {
		editor.add_command(new RemoveGraphObjectsCommand(editor, { link_id }, {}));
	}
}

void AnimationGraphEditorNew::init()
{
	PropertyFactoryUtil::register_basic(grid_factory);
	PropertyFactoryUtil::register_anim_editor2(*this,grid_factory);

	graph = make_unique<EditorNodeGraph>();
	graph->editor = this;

	settings = make_unique<AnimNodeGraphSettings>();
	tab_manager = make_unique<GraphTabManager>(*this);
	playback = make_unique<PlaybackManager>(*this);
	property_window = make_unique<GraphPropertyWindow>(*this);
	imnodes = make_unique<ImNodesInterface>();
	concmds = ConsoleCmdGroup::create("");
	concmds->add("anim.undo", [this](const Cmd_Args&) {
		cmd_manager.undo();
		});
	concmds->add("anim.del", [this](const Cmd_Args&) {
		delete_selected();
		});
	concmds->add("anim.dup", [this](const Cmd_Args&) {
		dup_selected();
		});
	concmds->add("anim.resolve_anys", [this](const Cmd_Args&) {
		resolve_any_types();
		});
	concmds->add("anim.up_layer", [this](const Cmd_Args&) {
		tab_manager->go_up_layer();
		});
	concmds->add("anim.down_layer", [this](const Cmd_Args&) {
		tab_manager->go_down_layer();
		});
	concmds->add("anim.copy", [this](const Cmd_Args&) {
		vector<int> links, nodes;
		GraphCommandUtil::get_selected(links, nodes);
		auto out = GraphCommandUtil::create_clipboard(nodes,*this);
		if (out.has_value())
			this->clipboard = out.value();
		});
	concmds->add("anim.paste", [this](const Cmd_Args&) {
		auto layer = tab_manager->get_active_tab();
		if(layer.has_value())
			add_command(new PasteNodeClipboardCommand(*this,*layer, this->clipboard));
		});


	on_node_changes.add(this, [this]() {
		resolve_any_types();
		graph->validate_nodes();
		});

	init_node_factory();
	params_window = make_unique<ControlParamsWindowNew>(*this);

	imnodes_context = ImNodes::CreateContext();
	//ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &is_modifier_pressed;
	ImNodes::GetStyle().Flags |= ImNodesStyleFlags_GridSnapping | ImNodesStyleFlags_GridLinesPrimary;
	if (!graph->get_root()) {
		auto root = graph->create_layer();
		graph->set_root(root);
		auto created = prototypes.create("ReturnPose");
		assert(created);
		graph->insert_new_node(*created, root->get_id(), std::nullopt);
	}
	assert(graph->get_root());
	tab_manager->open_tab(graph->get_root()->get_id(), true);
	on_selection_change.invoke();
}

void GraphTabManager::draw_popup_menu()
{
	auto does_match_type = [this](const string& creation_name) -> opt<int> {
		assert(from_drop.has_value());
		auto port = from_drop->link_node_to_this.get_port_ptr(editor.get_graph());
		if (!port) return std::nullopt;
		auto& type = port->type;
		auto template_type = MapUtil::get_opt(editor.get_prototypes().creations, creation_name);
		if(!template_type)
			template_type = MapUtil::get_opt(editor.get_var_prototypes().creations, creation_name);
		if (!template_type)
			return std::nullopt;
		assert(template_type->template_node);
		for (const auto& tport : template_type->template_node->ports) {
			if (tport.is_output() == from_drop->link_node_to_this.is_output())
				continue;
			if (tport.type == type)
				return tport.index;
			if (tport.type.type == GraphPinType::Any || type.type == GraphPinType::Any)
				return tport.index;
		}
		return std::nullopt;
	};
	auto draw_item = [this](bool is_dropping, auto&& does_match_type, NodeMenuItem& item) {
		if (item.color.has_value()) {
			ImGui::PushStyleColor(ImGuiCol_Text, item.color->to_uint());
		}
		if (is_dropping) {
			opt<int> matches = does_match_type(item.name);
			if (matches.has_value() && ImGui::MenuItem(item.name.c_str())) {
				opt<GraphLayerHandle> layer = get_active_tab();
				assert(layer.has_value());
				//editor.add_command(new AddNodeCommand(editor, item.name, mouse_click_pos, layer.value()));
				assert(from_drop.has_value());
				editor.add_command(new AddNodeWithLink(editor, item.name, mouse_click_pos, layer.value(), from_drop->link_node_to_this, matches.value()));
			}
		}
		else {
			if (ImGui::MenuItem(item.name.c_str())) {
				opt<GraphLayerHandle> layer = get_active_tab();
				assert(layer.has_value());
				editor.add_command(new AddNodeCommand(editor, item.name, mouse_click_pos, layer.value()));
			}
		}

		if (item.color.has_value()) {
			ImGui::PopStyleColor();
		}
	};

	const bool is_dropping = from_drop.has_value();
	if (set_keyboard_focus) {
		ImGui::SetKeyboardFocusHere();
		set_keyboard_focus = false;
	}
	if (ImGui::InputText("##text", (char*)node_menu_filter_buf.c_str(), node_menu_filter_buf.size() + 1, ImGuiInputTextFlags_CallbackResize, imgui_std_string_resize, &node_menu_filter_buf)) {
		node_menu_filter_buf = node_menu_filter_buf.c_str();
	}

	auto draw_menu = [is_dropping](auto&& self,auto&& does_match_type,auto&& draw_item, NodeMenu& menu) -> void {
		for (auto& item : menu.menus) {
			if (item.menu.has_value()) {
				if (ImGui::BeginMenu(item.name.c_str())) {
					self(self, does_match_type, draw_item,item.menu.value());
					ImGui::EndMenu();
				}
			}
			else {
				draw_item(is_dropping, does_match_type, item);
			}
		}
	};

	if(node_menu_filter_buf.empty())
		draw_menu(draw_menu, does_match_type, draw_item,editor.get_menu());
	else {
		vector<NodeMenuItem*> item_list;
		auto add_items = [](auto&& self, vector<NodeMenuItem*>& out, const string& filter, NodeMenu& menu) -> void {
			for (auto& item : menu.menus) {
				if (item.menu.has_value())
					self(self, out, filter, item.menu.value());
				else {
					auto lower = StringUtils::to_lower(item.name);
					if (lower.find(filter) !=  string::npos)
						out.push_back(&item);
				}
			}
		};
		auto lower = StringUtils::to_lower(node_menu_filter_buf);
		add_items(add_items, item_list, lower, editor.get_menu());
		for (auto& item : item_list) {
			draw_item(is_dropping, does_match_type, *item);
		}
	}
}
#include "Basic_nodes.h"
#include "Bttest.h"

void AnimationGraphEditorNew::init_node_factory()
{
	prototypes.add("+", []() { return new Math_EdNode(MathNodeType::Add); });
	prototypes.add("-", []() { return new Math_EdNode(MathNodeType::Sub); });
	prototypes.add("*", []() { return new Math_EdNode(MathNodeType::Mult); });
	prototypes.add("/", []() { return new Math_EdNode(MathNodeType::Div); });
	prototypes.add("==", []() { return new Math_EdNode(MathNodeType::Eq); });
	prototypes.add("!=", []() { return new Math_EdNode(MathNodeType::Neq); });
	prototypes.add("<=", []() { return new Math_EdNode(MathNodeType::Leq); });
	prototypes.add("<", []() { return new Math_EdNode(MathNodeType::Lt); });
	prototypes.add(">", []() { return new Math_EdNode(MathNodeType::Gt); });
	prototypes.add(">=", []() { return new Math_EdNode(MathNodeType::Geq); });
	prototypes.add("Or", []() { return new LogicalOp_EdNode(true); });
	prototypes.add("And", []() { return new LogicalOp_EdNode(false); });
	prototypes.add("Not", []() { return new Not_EdNode(); });



	prototypes.add("PlayClip", []() { return new Clip_EdNode(false); });
	prototypes.add("EvaluateClip", []() { return new Clip_EdNode(true); });
	prototypes.add("StateMachine", []() { return new Statemachine_EdNode(); });


	prototypes.add("Blend2", []() { return new ComposePoses_EdNode(false); });
	prototypes.add("AddPoses", []() { return new ComposePoses_EdNode(true); });
	prototypes.add("SubtractPoses", []() { return new SubtractPoses_EdNode(); });
	prototypes.add("MirrorPose", []() { return new MirrorPose_EdNode(); });
	prototypes.add("2DBlend", []() { return new Blend2D_EdNode(false); });
	prototypes.add("2DBlendAdd", []() { return new Blend2D_EdNode(true); });


	prototypes.add("BlendByInt", []() { return new BlendInt_EdNode(); });
	//prototypes.add("BlendByEnum", []() { return new BlendEnum_EdNode(); });
	prototypes.add("ReturnTransition", []() {return new Func_EdNode(Func_EdNode::ReturnTransition); });
	prototypes.add("ReturnPose", []() {return new Func_EdNode(Func_EdNode::ReturnPose); });

	prototypes.add("ModifyBone", []() {return new ModifyBone_EdNode(); });
	prototypes.add("Ik2Bone", []() {return new Ik2Bone_EdNode(); });

	prototypes.add("GetCurve", []() {return new Func_EdNode(Func_EdNode::GetCurve); });
	prototypes.add("IsEventActive", []() {return new Func_EdNode(Func_EdNode::IsEventActive); });
	prototypes.add("DidEventStart", []() {return new Func_EdNode(Func_EdNode::DidEventStart); });
	prototypes.add("DidEventEnd", []() {return new Func_EdNode(Func_EdNode::DidEventEnd); });
	prototypes.add("MakeVec3", []() {return new Func_EdNode(Func_EdNode::MakeVec3); });
	prototypes.add("BreakVec3", []() {return new Func_EdNode(Func_EdNode::BreakVec3); });
	prototypes.add("StateTimeRemaining", []() {return new Func_EdNode(Func_EdNode::StateTimeRemaining); });
	prototypes.add("StateDuration", []() {return new Func_EdNode(Func_EdNode::StateDuration); });



	prototypes.add("Variable", []() {return new Variable_EdNode(); });	// this assumes it got a variable name alredy
	prototypes.add("Comment", []() { return new CommentNode; });

	prototypes.add("Abs", []() {return new FloatMathFuncs_EdNode(FloatMathFuncs_EdNode::Abs); });
	prototypes.add("Clamp", []() {return new FloatMathFuncs_EdNode(FloatMathFuncs_EdNode::Clamp); });
	prototypes.add("InRange", []() {return new FloatMathFuncs_EdNode(FloatMathFuncs_EdNode::InRange); });
	prototypes.add("RemapFloat", []() {return new FloatMathFuncs_EdNode(FloatMathFuncs_EdNode::Remap); });
	prototypes.add("ScaleBias", []() {return new FloatMathFuncs_EdNode(FloatMathFuncs_EdNode::ScaleBias); });



	prototypes.add("State", []() {return new State_EdNode(); });
	prototypes.add("StateAlias", []() {return new StateAlias_EdNode(); });
	prototypes.add("EntryState", []() {return new Func_EdNode(Func_EdNode::EntryState); });

	prototypes.add("Parallel", []() {return new btParallel_EdNode(); });
	prototypes.add("Selector", []() {return new btComposite_EdNode(true); });
	prototypes.add("Sequence", []() {return new btComposite_EdNode(false); });

	prototypes.add("Filter", []() {return new btFilter_EdNode(); });
	prototypes.add("Action", []() {return new btAction_EdNode(); });
	prototypes.add("ConditionalLoop", []() {return new btFilter_EdNode(); });
	prototypes.add("TimeLimit", []() {return new btDecorator_EdNode(); });
	prototypes.add("Cooldown", []() {return new btDecorator_EdNode(); });





	NodeMenu mathmenu;
	mathmenu
		.add("+")
		.add("-")
		.add("*")
		.add("/")
		.add("Abs")
		.add("Clamp")
		.add("InRange")
		.add("RemapFloat")
		.add("ScaleBias");
	NodeMenu logcmp;
	logcmp
		.add("<")
		.add("<=")
		.add(">")
		.add(">=")
		.add("==")
		.add("!=")
		.add("Or")
		.add("And")
		.add("Not");
	NodeMenu blends;
	blends
		.add("Blend2")
		.add("2DBlend")
		.add("2DBlendAdd")
		.add("BlendByInt")
		.add("BlendByEnum")
		.add("AddPoses")
		.add("SubtractPoses")
		.add("MirrorPose");
	NodeMenu modifies;
	modifies
		.add("Ik2Bone")
		.add("ModifyBone");
	NodeMenu play;
	play
		.add("PlayClip")
		.add("EvaluateClip");

	NodeMenu funcs;
	funcs
		.add("GetCurve")
		.add("MakeVec3")
		.add("BreakVec3")
		.add("ReturnPose");
	NodeMenu transitionFuncs;
	transitionFuncs
		.add("IsEventActive")
		.add("DidEventStart")
		.add("DidEventEnd")
		.add("ReturnTransition")
		.add("StateTimeRemaining")
		.add("StateDuration");

	NodeMenu emptyVariables;

	animGraphMenu.add_submenu("Math", mathmenu);
	animGraphMenu.add_submenu("Logical", logcmp);
	animGraphMenu.add_submenu("Clips", play);
	animGraphMenu.add_submenu("Blends", blends);
	animGraphMenu.add_submenu("Modify", modifies);
	animGraphMenu.add_submenu("Transition", transitionFuncs);
	animGraphMenu.add_submenu("Misc", funcs);
	animGraphMenu.add_submenu("Variables", emptyVariables);

	animGraphMenu.add("StateMachine");
	animGraphMenu.add("State");
	animGraphMenu.add("StateAlias");
	animGraphMenu.add("EntryState");


	animGraphMenu.add("Comment");

	NodeMenu behaviors;
	behaviors
		.add("Sequence")
		.add("Parallel")
		.add("Selector")
		.add("Filter")
		.add("Action")
		.add("ConditionalLoop")
		.add("TimeLimit")
		.add("Cooldown");
	auto subclasses = ClassBase::get_subclasses<btDecorator>();
	for (; !subclasses.is_end(); subclasses.next()) {
		prototypes.add(subclasses.get_type()->classname, []() { return new btDecorator_EdNode(); });
		behaviors.add(subclasses.get_type()->classname);
	}

	//animGraphMenu.add_submenu("Behavior", behaviors);


}
void AnimationGraphEditorNew::delete_selected()
{
	vector<int> link_ids, node_ids;
	GraphCommandUtil::get_selected(link_ids, node_ids);
	add_command(new RemoveGraphObjectsCommand(*this, link_ids, node_ids));

}
void AnimationGraphEditorNew::dup_selected()
{
	vector<int> link_ids, node_ids;
	GraphCommandUtil::get_selected(link_ids, node_ids);
	add_command(new DuplicateNodesCommand(*this, node_ids));
}

void AnimationGraphEditorNew::close_internal()
{
}

void AnimationGraphEditorNew::tick(float dt)
{
	EditorTool3d::tick(dt);

}

void AnimationGraphEditorNew::hook_menu_bar()
{
}

bool AnimationGraphEditorNew::can_save_document()
{
	return true;
}

void AnimationGraphEditorNew::imgui_draw()
{
	auto check_open_tab = [this]() {
		Base_EdNode* const selected = get_selected_node();
		const bool doubleclicked = ImGui::GetIO().MouseClickedCount[0] == 2;
		if (doubleclicked && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_RootWindow) && selected) {
			if (selected->get_owning_sublayer().is_valid()) {
				const auto sublayerhandle = selected->get_owning_sublayer();
				tab_manager->open_tab(sublayerhandle, true);
				ImNodes::ClearNodeSelection();
			}
		}
	};

	if(ImGui::Begin("Graph",nullptr, ImGuiWindowFlags_NoNav)) {
		playback->draw();
		tab_manager->draw();
		check_open_tab();
	}
	ImGui::End();

	draw_layer_window();

	property_window->draw();
	params_window->imgui_draw();
	cmd_manager.execute_queued_commands();

	auto update_selected = [this]() {
		Base_EdNode* nextselected = get_selected_node();
		if (!nextselected) {
			if (selected_last_frame.is_valid()) {
				selected_last_frame = GraphNodeHandle();
				on_selection_change.invoke();
			}
		}
		else if (!(nextselected->self == selected_last_frame)) {
			selected_last_frame = nextselected->self;
			on_selection_change.invoke();
		}
		vector<int> n, l;
		GraphCommandUtil::get_selected(l, n);
		multiple_selected_last_frame.clear();
		for (int node : n) {
			multiple_selected_last_frame.insert(node);
		}
	};
	update_selected();
}

bool AnimationGraphEditorNew::save_document_internal()
{
	return false;
}

const ClassTypeInfo& AnimationGraphEditorNew::get_asset_type_info() const
{
	return Animation_Tree_CFG::StaticType;
}

const char* AnimationGraphEditorNew::get_save_file_extension() const
{
	return "ag";
}
void GraphTabManager::draw()
{
	int rendered = 0;

	if (tabs.empty())
		return;

	auto forward_img = g_assets.find_global_sync<Texture>("icon/forward.png");
	auto back_img = g_assets.find_global_sync<Texture>("icon/back.png");


	bool wants_back = (ImGui::IsWindowFocused() && !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_LeftArrow));
	bool wants_forward = (ImGui::IsWindowFocused() && !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_RightArrow));

	//ImGui::BeginDisabled(active_tab_hist.empty());
	//if (ImGui::ImageButton(ImTextureID(uint64_t(back_img->gl_id)), ImVec2(16, 16)) || (wants_back && !active_tab_hist.empty())) {
	//
	//	forward_tab_stack.push_back(active_tab);
	//	active_tab = active_tab_hist.back();
	//	active_tab_hist.pop_back();
	//	active_tab_dirty = true;
	//}
	//ImGui::EndDisabled();
	//ImGui::SameLine();
	//ImGui::BeginDisabled(forward_tab_stack.empty());
	//if (ImGui::ImageButton(ImTextureID(uint64_t(forward_img->gl_id)), ImVec2(16, 16)) || (wants_forward && !forward_tab_stack.empty())) {
	//	active_tab_hist.push_back(active_tab);
	//	active_tab = forward_tab_stack.back();
	//	forward_tab_stack.pop_back();
	//	active_tab_dirty = true;
	//}
	//ImGui::EndDisabled();

	// keeps a history of what that last tab actually rendered was
	static int actual_last_tab_rendered = -1;

	unordered_set<int> delete_indicies;

	EditorNodeGraph& graph = editor.get_graph();

	if (ImGui::BeginTabBar("tabs")) {
		for (int n = 0; n < tabs.size(); n++) {
			bool needs_select = n == active_tab && active_tab_dirty;
			auto flags = (needs_select) ? ImGuiTabItemFlags_SetSelected : 0;

			NodeGraphLayer* const layer = graph.get_layer(tabs.at(n));
			if (!layer) {
				LOG_WARN("no layer");
				delete_indicies.insert(n);
				continue;
			}


			bool open_bool = true;
			ImGui::PushID(layer);
			auto[tabname,backcolor] = layer->get_tab_name(graph);
			if (ImGui::BeginTabItem(string_format("%s", tabname.c_str()), &open_bool, flags))
			{
				bool this_is_an_old_active_tab_or_just_skip = n != active_tab && active_tab_dirty;

				if (this_is_an_old_active_tab_or_just_skip) {
					ImGui::EndTabItem();
					ImGui::PopID();
					continue;
				}

				bool this_tab_needs_a_reset = n != active_tab;

				//uint32_t layer = (tabs[n].layer) ? tabs[n].layer->id : 0;
				ImNodesEditorContext* const context = layer->get_context();
				if (this_tab_needs_a_reset) {
					ImNodes::ClearNodeSelection();
				}
				ImNodes::EditorContextSet(context);
				if (this_tab_needs_a_reset) {
					ImNodes::ClearNodeSelection();
				}
				ImNodes::PushColorStyle(ImNodesCol_GridBackground, backcolor.to_uint());
				layer->draw(graph);
				ImNodes::PopColorStyle();

				//parent->draw_graph_layer(layer);

				rendered++;
				ImGui::EndTabItem();

				if (active_tab != n) {
					ASSERT(!active_tab_dirty);
					active_tab = n;
				}
			}
			ImGui::PopID();
			if (!open_bool) {
				delete_indicies.insert(n);
			}
		}
		ImGui::EndTabBar();
	}

	active_tab_dirty = false;

	if (delete_indicies.size() > 0) {
		vector<GraphLayerHandle> next;
		for (int i = 0; i < tabs.size(); i++) {
			if (delete_indicies.find(i) == delete_indicies.end()) {
				next.push_back(tabs[i]);
				if (active_tab.has_value() && active_tab == i)
					active_tab = (int)next.size() - 1;
			}
		}
		this->tabs = next;
	}

	handle_link_changes();

	const bool is_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootWindow);
	if (is_focused && ImGui::GetIO().MouseClicked[1]) {
		ImGui::OpenPopup("my_select_popup");
		mouse_click_pos = GraphUtil::to_glm(ImGui::GetMousePos());
		set_keyboard_focus = true;
	}
	if (ImGui::BeginPopup("my_select_popup"))
	{
		draw_popup_menu();
		ImGui::EndPopup();
	}
	else {
		from_drop = std::nullopt;
		node_menu_filter_buf.clear();
	}

}

void GraphTabManager::open_tab(GraphLayerHandle handle, bool set_active)
{
	history.clear();
	tabs.clear();
	tabs.push_back(handle);
	active_tab = 0;
	active_tab_dirty = true;
}
void GraphTabManager::go_up_layer()
{
	if (tabs.empty())
		return;
	auto t = tabs[0];
	auto layer = editor.get_graph().get_layer(t);
	if (layer) {
		auto owner = editor.get_graph().get_node(layer->get_owner_node());
		if (owner) {
			history.push_back(t);
			tabs.clear();
			tabs.push_back(owner->layer);
			active_tab_dirty = true;
			active_tab = 0;
		}
	}

}
void GraphTabManager::go_down_layer()
{
	if (!history.empty()) {
		auto b = history.back();
		history.pop_back();
		tabs.clear();
		tabs.push_back(b);
		active_tab = 0;
		active_tab_dirty = true;
	}
}

void GraphTabManager::close_tab(GraphLayerHandle handle)
{
}
/*

	// Handle Hovered links
	int link = 0;
	if (ImNodes::IsLinkHovered(&link)) {


		int node_id = Base_EdNode::get_nodeid_from_link_id(link);
		int slot = Base_EdNode::get_slot_from_id(link);
		Base_EdNode* node_s = anim_graph_ed.find_node_from_id(node_id);
		if (node_s->is_state_node()) {

			auto state = dynamic_cast<State_EdNode*>(node_s);
			ASSERT(state);

			if (state->inputs[slot].node) {
				ImGui::BeginTooltip();
				auto other_node = state->inputs[slot].node->cast_to<State_EdNode>();
				ASSERT(other_node);
				// should always be true
				auto other_name = other_node->get_title();
				auto my_name = state->get_title();

				ImGui::Text("%s ->\n%s", other_name.c_str(), my_name.c_str());
				ImGui::Separator();

				auto st = other_node->get_state_transition_to(state, slot);
				ASSERT(st);

				if (st->is_a_continue_transition())
					ImGui::TextColored(ImVec4(0.2, 1.0, 0.2, 1.0), "CONTINUE");
				else
					ImGui::TextColored(ImVec4(1.0, 0.3, 0.3, 1.0), "read conditions");

				ImGui::EndTooltip();
			}

		}

	}

*/
static void draw_enum_editor(int& myval, const EnumTypeInfo* type) {
	assert(type);
	const EnumIntPair* myenumint = type->find_for_value(myval);
	if (!myenumint) {
		assert(type->str_count > 0);
		myval = type->strs[0].value;
		myenumint = &type->strs[0];
	}
	if (ImGui::BeginCombo("##type", myenumint->name)) {
		for (auto& enumiterator : *type) {
			bool selected = (int)enumiterator.value == myval;
			if (ImGui::Selectable(enumiterator.name, &selected)) {
				myval = (int)enumiterator.value;
			}
		}
		ImGui::EndCombo();
	}
}


#include "GraphUtil.h"
void Base_EdNode::draw_imnode()
{
	if (is_link_attached_node())
		return;

	const Color32 nodecolor = get_node_color();
	const string title = get_title();
	const string subtitle = get_subtitle();


	ImNodes::PushColorStyle(ImNodesCol_TitleBar, nodecolor.to_uint());
	Color32 select_color = GraphUtil::add_brightness(nodecolor, 30);
	Color32 hover_color = GraphUtil::add_brightness(GraphUtil::mix_with(nodecolor, { 5, 225, 250 }, 0.6), 5);
	ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, hover_color.to_uint());
	ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, select_color.to_uint());


	// node is selected
	const bool is_selected = editor->is_node_selected(*this);

	if (is_selected) {
		ImNodes::PushColorStyle(ImNodesCol_NodeOutline, GraphUtil::color32_to_int({ 255, 174, 0 }));
		ImNodes::PushStyleVar(ImNodesStyleVar_NodeBorderThickness, 2.0);
	}

	ImNodes::BeginNode(self.id, draw_links_as_arrows());
	Texture* t = g_assets.find_global_sync<Texture>("eng/editor/node_titlebar2.png").get();
	if (has_top_bar()) {
		ImNodes::BeginNodeTitleBar(ImTextureID(uint64_t(t->gl_id)));
		{
			auto cursorpos = ImGui::GetCursorPos();
			ImGui::Dummy(ImVec2(100, 0.0));
			ImGui::SameLine(0, 0);
			ImGui::SetCursorPosX(cursorpos.x);
			ImGui::Text("%s\n", title.c_str());

			if (!subtitle.empty()) {
				ImGui::TextColored(ImColor(190, 190, 190), "%s", subtitle.c_str());
			}
		}

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && ImGui::BeginTooltip()) {
			auto drawlist = ImGui::GetForegroundDrawList();
			auto& style = ImGui::GetStyle();
			auto min = ImGui::GetCursorScreenPos();
			const string& tooltip = get_tooltip();
			auto sz = ImGui::CalcTextSize(tooltip.c_str());
			float width = ImGui::CalcItemWidth();

			auto minrect = ImVec2(min.x - style.FramePadding.x * 5.0, min.y);
			auto maxrect = ImVec2(min.x + sz.x + style.FramePadding.x * 5.0, min.y + sz.y + style.FramePadding.y * 2.0);
			float border = 2.0;
			drawlist->AddRectFilled(ImVec2(minrect.x - border, minrect.y - border), ImVec2(maxrect.x + border, maxrect.y + border), Color32{ 0,0,0, 255 }.to_uint());


			drawlist->AddRectFilled(minrect, maxrect, Color32{ 255,255,255, 255 }.to_uint());
			auto cursor = ImGui::GetCursorScreenPos();
			drawlist->AddText(cursor, Color32{ 0,0,0 }.to_uint(), tooltip.c_str());
			//ImGui::Text(str.c_str());
			//ImGui::Text(node->get_tooltip().c_str());
			ImGui::EndTooltip();
		}

#pragma warning(disable: 4312)
		//if (!node->compile_error_string.empty()) {
		//	ImGui::SameLine();
		//	ImGui::Image((ImTextureID)strong_error->gl_id, ImVec2(16, 16));
		//
		//	if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
		//		ImGui::Text(node->compile_error_string.c_str());
		//		ImGui::EndTooltip();
		//	}
		//
		//}
		//
		//if (node->children_have_errors) {
		//	ImGui::SameLine();
		//	ImGui::Image((ImTextureID)weak_error->gl_id, ImVec2(16, 16));
		//
		//	if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
		//		ImGui::Text("children have errors");
		//		ImGui::EndTooltip();
		//	}
		//}
		//
		//if (!node->compile_info_string.empty()) {
		//	ImGui::SameLine();
		//	ImGui::Image((ImTextureID)info_img->gl_id, ImVec2(16, 16));
		//
		//	if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
		//		ImGui::Text(node->compile_info_string.c_str());
		//		ImGui::EndTooltip();
		//	}
		//}
#pragma warning(default: 4312)


		ImNodes::EndNodeTitleBar();
	}
	else {
		ImGui::Dummy(ImVec2(0, 0.0));
		ImGui::SameLine(0, 0);
	}

	float x1 = ImGui::GetItemRectMin().x;
	float x2 = ImGui::GetItemRectMax().x;

	vector<int> input_ports;
	vector<int> output_ports;
	get_ports(input_ports, output_ports);
	const float topy = ImGui::GetCursorPosY();
	for (int input_idx : input_ports) {
		GraphPort& port = ports.at(input_idx);
		const GraphPortHandle phandle = port.get_handle(self);
		const ImNodesPinShape pin = push_and_get_pin_type(port.type.type, find_link_from_port(phandle).has_value());
		opt<GraphLink> link = find_link_from_port(phandle);

		//if (link.has_value())
		//	pin = ImNodesPinShape_TriangleFilled;
		ImNodes::BeginInputAttribute(phandle.id, pin);
		if (!draw_links_as_arrows()) {
			const string& str = port.name;
			ImGui::Text("%s", str.c_str());
			using std::holds_alternative;
			if (!link.has_value()) {
				ImGui::PushItemWidth(90);
				if (str.empty())
					ImGui::SameLine();

				if (port.type.type == GraphPinType::Boolean) {
					if (!holds_alternative<bool>(port.inlineValue))
						port.inlineValue = false;
					bool b = std::get<bool>(port.inlineValue);
					ImGui::Checkbox("##b", &b);
					port.inlineValue = b;
				}
				else if (port.type.type == GraphPinType::Float) {
					if (!holds_alternative<float>(port.inlineValue))
						port.inlineValue = 0.f;
					float b = std::get<float>(port.inlineValue);
					ImGui::InputFloat("##b", &b);
					port.inlineValue = b;
				}
				else if (port.type.type == GraphPinType::Integer) {
					if (!holds_alternative<int>(port.inlineValue))
						port.inlineValue = 0;
					int i = std::get<int>(port.inlineValue);
					ImGui::InputInt("##", &i);
					port.inlineValue = i;
				}
				else if (port.type.type == GraphPinType::Vec3) {
					if (!holds_alternative<glm::vec3>(port.inlineValue))
						port.inlineValue = glm::vec3(0.f);
					glm::vec3 v = std::get<glm::vec3>(port.inlineValue);
					ImGui::InputFloat3("##", &v.x);
					port.inlineValue = v;
				}
				else if (port.type.type == GraphPinType::EnumType && holds_alternative<const EnumTypeInfo*>(port.type.data)) {
					auto enumtype = std::get<const EnumTypeInfo*>(port.type.data);
					if (enumtype) {
						if (!holds_alternative<int>(port.inlineValue))
							port.inlineValue = 0;
						int i = std::get<int>(port.inlineValue);
						draw_enum_editor(i, std::get<const EnumTypeInfo*>(port.type.data));
						port.inlineValue = i;
					}
				}

				ImGui::PopItemWidth();
			}
		}
		ImNodes::EndInputAttribute();

		ImNodes::PopColorStyle();
	}
	ImGui::SetCursorPosY(topy);
	for (int output_idx : output_ports) {
		const GraphPort& port = ports.at(output_idx);
		const GraphPortHandle phandle = port.get_handle(self);
		const ImNodesPinShape pin = push_and_get_pin_type(port.type.type, find_link_from_port(phandle).has_value());

		ImNodes::BeginOutputAttribute(phandle.id, pin);
		const string& output_str = port.name;

		auto posX = (ImGui::GetCursorPosX() + (x2 - x1) - ImGui::CalcTextSize(output_str.c_str()).x - 2 * ImGui::GetStyle().ItemSpacing.x) - 5.0;
		if (posX > ImGui::GetCursorPosX())
			ImGui::SetCursorPosX(posX);

		//auto output_type = node->get_output_type_general();

		//ImGui::TextColored(graph_pin_type_to_color(output_type), output_str.c_str());
		ImGui::Text("%s", output_str.c_str());
		ImNodes::EndOutputAttribute();

		ImNodes::PopColorStyle();
	}
	ImNodes::EndNode();


	{
		glm::vec2 pos = GraphUtil::to_glm(ImNodes::GetNodeGridSpacePos(self.id));
		this->nodex = pos.x;
		this->nodey = pos.y;
	}

	ImNodes::PopColorStyle();
	ImNodes::PopColorStyle();
	ImNodes::PopColorStyle();

	if (is_selected) {
		ImNodes::PopStyleVar();
		ImNodes::PopColorStyle();
	}

	//bool draw_flat_links = node->draw_flat_links();
	for (int input_idx : input_ports) {
		const GraphPort& port = ports.at(input_idx);
		GraphPortHandle phandle = port.get_handle(self);
		opt<GraphLink> link = find_link_from_port(phandle);
		
		if (link.has_value()) {
			GraphLink& lv = link.value();

			// n^2 :(
			int offset = 0;
			const bool as_arrows = draw_links_as_arrows();
			if (as_arrows) {
				for (int subindex : input_ports) {
					if (subindex == input_idx)
						break;
					opt<GraphLink> sublink = find_link_from_port(ports.at(subindex).get_handle(self));
					if (sublink.has_value() && sublink.value().get_other_node(self) == lv.get_other_node(self))
						offset += 1;
				}
			}


			//bool pushed_colors = node->push_imnode_link_colors(j);
			auto [color, type] = GraphUtil::get_pin_for_value_type(port.type.type);
			ImNodes::PushColorStyle(ImNodesCol_Link, color.to_uint());
			//ImNodes::Link(node->getlink_id(j), node->inputs[j].node->getoutput_id(0), node->getinput_id(j), draw_flat_links, offset);
			
			ImNodes::Link(lv.get_link_id(), lv.input.id, lv.output.id, as_arrows, offset);
			ImNodes::PopColorStyle();
			//if (pushed_colors) {
			//	ImNodes::PopColorStyle();
			//	ImNodes::PopColorStyle();
			//	ImNodes::PopColorStyle();
			//
			//}
		}
	}
}
void NodeGraphLayer::draw(EditorNodeGraph& graph)
{

	auto winsize = ImGui::GetWindowSize();
	if (wants_reset_view) {
		ImNodes::EditorContextResetPanning(ImVec2(0, 0));
		ImNodes::EditorContextResetPanning(ImVec2(winsize.x / 2, winsize.y / 2));
		wants_reset_view = false;
	}

	auto strong_error = g_assets.find_global_sync<Texture>("icon/fatalerr.png");
	auto weak_error = g_assets.find_global_sync<Texture>("icon/error.png");
	auto info_img = g_assets.find_global_sync<Texture>("icon/question.png");

	ImNodes::BeginNodeEditor();
	for (int _node_id : layer_nodes) {
		GraphNodeHandle node_handle(_node_id);
		Base_EdNode* node = graph.get_node(GraphNodeHandle(_node_id));
		if (!node) {
			LOG_WARN("no node in layer");
			continue;
		}
		node->draw_imnode();
	}

	ImNodes::MiniMap();
	ImNodes::EndNodeEditor();


	handle_drag_drop();

}
void EditorNodeGraph::remove_layer(NodeGraphLayer* layer)
{
	assert(layer);
	sys_print(Debug, "removing layer: %d (owned: %d)\n", layer->get_id().id, layer->get_owner_node().id);
	unordered_set<int> copied_ids = layer->get_nodes();
	for (int i : copied_ids) {
		remove_node(GraphNodeHandle(i));
	}
	assert(layer->get_nodes().size() == 0);
	const int id = layer->get_id().id;
	assert(layers.find(id) != nullptr);
	layers.remove(id);
	delete layer;
}
void EditorNodeGraph::remove_node(GraphNodeHandle handle) {
	if (!handle.is_valid())
		return;
	sys_print(Debug, "removing node: %d\n", handle.id);
	auto node = get_node(handle);
	if (!node) {
		sys_print(Warning, "node not found: %d\n", handle.id);
		return;
	}
	
	{
		int size = node->links.size();
		while (!node->links.empty())
		{
			GraphLinkWithNode linkwithnode = node->links.back();
			if (linkwithnode.opt_link_node.is_valid())
				remove_node(linkwithnode.opt_link_node);
			GraphCommandUtil::remove_link(linkwithnode.link, *this);
			if (node->links.size() >= size) {
				// didnt delte?
				printf("link didn't delete\n");
				node->links.pop_back();
			}
			assert(node->links.size() < size);
			size = node->links.size();
		}
	}
	auto layer = get_layer(node->layer);
	if (!layer) {
		sys_print(Warning, "nodes layer not found %d\n", node->layer.id);
	}
	else {
		layer->remove_node(*node);
	}
	if (node->get_owning_sublayer().is_valid()) {
		remove_layer(get_layer(node->get_owning_sublayer()));
	}

	nodes.remove(handle.id);
	delete node;
}
void EditorNodeGraph::insert_new_node(Base_EdNode& node, GraphLayerHandle layer, opt<glm::vec2> pos) {
	printf("insert new node\n");
	auto layerptr = get_layer(layer);
	assert(layerptr);
	node.self = GraphNodeHandle(get_next_id());
	node.layer = layer;
	node.editor = editor;
	nodes.insert(node.self.id, &node);
	layerptr->add_node_to_layer(node);
	if (pos.has_value()) {
		ImNodes::SetNodeScreenSpacePos(node.self.id, GraphUtil::to_imgui(pos.value()));
		ImVec2 v = ImNodes::GetNodeGridSpacePos(node.self.id);
		node.nodex = v.x;
		node.nodey = v.y;
	}
	else {
		node.nodex = node.nodey = 0.f;
	}
}
void EditorNodeGraph::validate_nodes()
{
	// rules:
	// every container must be owned by someone
	// nodes must be in valid container
	assert(editor);
	assert(root_layer);
	assert(get_layer(root_layer->get_id()) == root_layer);
	unordered_set<Base_EdNode*> seen_link_nodes;
	for (auto n : nodes) {
		assert(get_node(n->self) == n);
		assert(n->editor == editor);
		auto layerptr = get_layer(n->layer);
		assert(layerptr);
		assert(layerptr->contains(n->self.id));
		auto sublayer = n->get_owning_sublayer();
		if (sublayer.is_valid()) {
			auto sublayerptr = get_layer(sublayer);
			assert(sublayerptr && sublayerptr->get_owner_node() == n->self);
			assert(sublayerptr != layerptr);
		}
		for (auto& l : n->links) {
			auto in = l.link.input.get_node_ptr(*this);
			auto out = l.link.output.get_node_ptr(*this);
			assert(in&&out);
			assert(in->layer == out->layer);
			assert(in != out);
			if (l.opt_link_node.is_valid()) {
				assert(in == n);
				auto linknode = get_node(l.opt_link_node);
				assert(linknode);
				assert(linknode->is_link_attached_node());
				assert(linknode->layer == n->layer);
				assert(!SetUtil::contains(seen_link_nodes, linknode));
				seen_link_nodes.insert(linknode);
			}
			auto inport = l.link.input.get_port_ptr(*this);
			auto outport = l.link.output.get_port_ptr(*this);
			assert(inport && outport);
			assert(inport->is_input() && outport->is_output());
		}
		assert(!n->is_link_attached_node()||n->links.empty());
	}
	for (auto n : nodes) {
		assert(n->is_link_attached_node() == SetUtil::contains(seen_link_nodes, n));
	}
	for (auto c : layers) {
		assert(get_layer(c->get_id()) == c);
		
		if (c == root_layer) {
			assert(!c->get_owner_node().is_valid());
		}
		else {
			auto owner = c->get_owner_node();
			auto ownerptr = get_node(owner);
			assert(ownerptr && ownerptr->get_owning_sublayer() == c->get_id());
		}
		
		for (int i : c->get_nodes()) {
			auto subnode = get_node(GraphNodeHandle(i));
			assert(subnode && subnode->layer == c->get_id());
		}
	}
};

void EditorNodeGraph::insert_nodes(SerializeGraphContainer& container)
{
	assert(root_layer);
	for (auto n : container.layers) {
		assert(n && n->get_id().is_valid());
		int id = n->get_id().id;
		assert(layers.find(id) == nullptr);
		layers.insert(id, n);
	}
	for (auto n : container.nodes) {
		assert(n && n->self.is_valid());
		assert(nodes.find(n->self.id) == nullptr);
		nodes.insert(n->self.id, n);
		n->editor = editor;
		auto layer = get_layer(n->layer);
		if (!layer) {
			sys_print(Warning, "layer not found for unserialized node\n");
			n->layer = root_layer->get_id();
			layer = root_layer;
		}
		layer->add_node_to_layer(*n);
		ImNodes::SetNodeGridSpacePos(n->self.id, ImVec2(n->nodex, n->nodey));
	}
	for (Base_EdNode* n : container.nodes) {
		// fixup links for nodes that werent deleted
		for (GraphLinkWithNode l : n->links) {
			GraphCommandUtil::add_link(l.link, *this, GraphNodeHandle());
		}
	}
}
#include "Framework/PropertyPtr.h"

template<typename FUNCTOR>
inline void visit_all_properties_and_apply_functor(ClassBase* b, FUNCTOR&& func)
{
	auto recurse = [](auto&& self, auto&& func, PropertyPtr prop) -> void {
		func(prop);
		if (prop.is_array()) {
			for (auto p : prop.as_array()) {
				self(self, func, p);
			}
		}
		else if (prop.is_struct()) {
			for (auto p : prop.as_struct()) {
				self(self, func, p);
			}
		}
	};
	for (auto prop : ClassPropPtr(b)) {
		recurse(recurse, func, prop);
	}
}



void EditorNodeGraph::insert_nodes_with_new_id(SerializeGraphContainer& container)
{
	unordered_map<int, int> old_id_to_new_id;
	assert(root_layer);
	for (auto n : container.layers) {
		assert(n && n->get_id().is_valid());
		const int oldid = n->get_id().id;
		n->set_id(get_next_id());
		const int id = n->get_id().id;
		old_id_to_new_id.insert({ oldid,id });
		assert(layers.find(id) == nullptr);
		layers.insert(id, n);
	}
	for (auto n : container.nodes) {
		assert(n && n->self.is_valid());
		const int oldid = n->self.id;
		n->self.id = get_next_id();
		n->editor = editor;
		old_id_to_new_id.insert({ oldid,n->self.id });
		assert(layers.find(n->self.id) == nullptr);
		nodes.insert(n->self.id, n);
		if(MapUtil::contains(old_id_to_new_id, n->layer.id))
			n->layer = old_id_to_new_id.find(n->layer.id)->second;
		auto layer = get_layer(n->layer);
		if (!layer) {
			sys_print(Warning, "layer not found for unserialized node\n");
			n->layer = root_layer->get_id();
			layer = root_layer;
		}
		layer->add_node_to_layer(*n);

		if (n->get_owning_sublayer().is_valid()) {
			const int subid = n->get_owning_sublayer().id;
			assert(MapUtil::contains(old_id_to_new_id,subid));
			const int duplayer = old_id_to_new_id.find(subid)->second;
			NodeGraphLayer* layer = get_layer(duplayer);
			assert(layer);
			layer->set_owner_node(n->self);
			n->set_owning_sublayer(duplayer);
			assert(n->get_owning_sublayer().id == duplayer);
		}

		ImNodes::SetNodeGridSpacePos(n->self.id, ImVec2(n->nodex, n->nodey));
	}
	vector<GraphNodeHandle> handles;
	for (auto n : container.nodes)
		handles.push_back(n->self);

	for (auto h : handles) {
		auto n = get_node(h);
		if (!n)
			continue;

		for (int i = 0; i < (int)n->links.size(); i++) {
			GraphLinkWithNode& l = n->links.at(i);
			if (l.opt_link_node.is_valid()) {
				assert(MapUtil::contains(old_id_to_new_id, l.opt_link_node.id));
				l.opt_link_node = old_id_to_new_id.find(l.opt_link_node.id)->second;
			}
			GraphNodeHandle inp = l.link.input.get_node();
			GraphNodeHandle out = l.link.output.get_node();
			if (MapUtil::contains(old_id_to_new_id, inp.id) && MapUtil::contains(old_id_to_new_id, out.id)) {
				l.link.input = GraphPortHandle::make(old_id_to_new_id.find(inp.id)->second, l.link.input.get_index(), false);
				l.link.output = GraphPortHandle::make(old_id_to_new_id.find(out.id)->second, l.link.output.get_index(), true);
			}
			else {
				printf("removing link not found\n");
				if (l.opt_link_node.is_valid())
					remove_node(l.opt_link_node);
				n->links.erase(n->links.begin() + i);
				i--;
			}
		}
		n->fixup_any_extra_references(old_id_to_new_id);
	}
}
void NodeGraphLayer::handle_drag_drop()
{
	if (ImGui::BeginDragDropTarget())
	{
		//bool is_sm = graph_tabs->get_active_tab()->is_statemachine_tab();
		//uint32_t layer = graph_tabs->get_current_layer_from_tab();
		//if (!is_sm) {
		//	if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop"))
		//	{
		//		AssetOnDisk* resource = *(AssetOnDisk**)payload->Data;
		//		if (resource->type->get_asset_class_type() == &AnimationSeqAsset::StaticType) {
		//			auto node = user_create_new_graphnode(Clip_EdNode::StaticType.classname, layer);
		//			Clip_EdNode* cl = node->cast_to<Clip_EdNode>();
		//			ASSERT(cl);
		//			cl->node->Clip = g_assets.find_sync<AnimationSeqAsset>(resource->filename);
		//			ImNodes::ClearNodeSelection();
		//			ImNodes::SetNodeScreenSpacePos(cl->id, ImGui::GetMousePos());
		//			ImNodes::SelectNode(cl->id);
		//		}
		//	}
		//	else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AnimGraphVariableDrag")) {
		//		VariableNameAndType* var = *(VariableNameAndType**)payload->Data;
		//		auto node = user_create_new_graphnode(Variable_EdNode::StaticType.classname, layer);
		//		Variable_EdNode* v = node->cast_to<Variable_EdNode>();
		//		ASSERT(v);
		//
		//		v->variable = *var;
		//
		//		ImNodes::ClearNodeSelection();
		//		ImNodes::SetNodeScreenSpacePos(v->id, ImGui::GetMousePos());
		//		ImNodes::SelectNode(v->id);
		//	}
		//}
		ImGui::EndDragDropTarget();
	}
}

GraphPropertyWindow::GraphPropertyWindow(AnimationGraphEditorNew& editor)
	: grid(editor.get_factory()), ed(editor)
{
	editor.on_node_changes.add(this, [this]() {
			update_property_window();
		});
	editor.on_selection_change.add(this, [this]() {
		update_property_window();
		});
}
void GraphPropertyWindow::update_property_window()
{
	printf("clear grid\n");
	grid.clear_all();
	Base_EdNode* node = ed.get_selected_node();
	if (!node) {
		grid.add_class_to_grid(ed.get_options_ptr());
	}
	else {
		grid.add_class_to_grid(node);
	}
}

void ImNodesInterface::set_node_position(GraphNodeHandle self, glm::vec2 pos)
{
	ImNodes::SetNodeScreenSpacePos(self.id, GraphUtil::to_imgui(pos));
}
static GraphPortHandle getinput_id(GraphNodeHandle self, int inputslot) {
	return inputslot + self.id * MAX_INPUTS + INPUT_START;
}
static GraphPortHandle getoutput_id(GraphNodeHandle self, int outputslot) {
	return outputslot + self.id * MAX_INPUTS + OUTPUT_START;
}
GraphPortHandle GraphPortHandle::make(GraphNodeHandle node, int index, bool is_output)
{
	return is_output ? getoutput_id(node, index) : getinput_id(node, index);
}
GraphPortHandle GraphPort::get_handle(GraphNodeHandle node) const
{
	return GraphPortHandle::make(node, index, is_output());
}

#include "Framework/SerializerJson.h"

template<typename T>
void serialize_set_of_ptrs(Serializer& s, const char* tag, unordered_set<T*>& set)
{
	int size = set.size();
	s.serialize_array(tag, size);
	if (s.is_saving()) {
		for (auto l : set) {
			s.serialize_class_ar<T>(l);
		}
	}
	else {
		for (int i = 0; i < size;i++) {
			T* ptr = nullptr;
			s.serialize_class_ar<T>(ptr);
			if (ptr)
				SetUtil::insert_test_exists(set, ptr);
		}
	}
	s.end_obj();
}

void SerializeGraphContainer::serialize(Serializer& s)
{
	serialize_set_of_ptrs(s, "layers", layers);
	serialize_set_of_ptrs(s, "nodes", nodes);
}

class MakePathForAnimNode : public IMakePathForObject {
public:
	MakePathForAnimNode(const NodePrototypes& p) : p(p) {}
	MakePath make_path(const ClassBase* toobj) final {
		return "x" + std::to_string(uintptr_t(toobj));
	}
	std::string make_type_name(ClassBase* obj) final {
		if (auto ednode = obj->cast_to<Base_EdNode>()) {
			if(!ednode->name.empty())
				return ednode->name;
		}
		return obj->get_type().classname;
	}
	nlohmann::json* find_diff_for_obj(ClassBase* obj) final { return nullptr; }
private:
	const NodePrototypes& p;
};
class MakeObjectFromAnimNode : public IMakeObjectFromPath {
public:
	MakeObjectFromAnimNode(const NodePrototypes& p) : p(p) {}
	ClassBase* create_from_name(ReadSerializerBackendJson& s, const std::string& str, const string& parentpath) final {
		if (MapUtil::contains(p.creations, str)) {
			return p.create(str);
		}
		return ClassBase::create_class<ClassBase>(str.c_str());
	}
	const NodePrototypes& p;
};


uptr<SerializeGraphContainer> SerializeGraphUtils::unserialize(const string& text, const NodePrototypes& p)
{
	MakeObjectFromAnimNode objmaker(p);
	ReadSerializerBackendJson writer(text, objmaker, *AssetDatabase::loader);
	ClassBase* rootobj = writer.get_root_obj();
	if (rootobj&&rootobj->cast_to<SerializeGraphContainer>()) {
		return uptr<SerializeGraphContainer>(rootobj->cast_to<SerializeGraphContainer>());
	}
	sys_print(Warning, "couldnt load graph\n");
	delete rootobj;
	return nullptr;
}

string SerializeGraphUtils::serialize_to_string(SerializeGraphContainer& container, EditorNodeGraph& graph, const NodePrototypes& p)
{
	MakePathForAnimNode pathmaker(p);
	WriteSerializerBackendJson writer(pathmaker,container);
	return writer.get_output().dump();
}

SerializeGraphContainer SerializeGraphUtils::make_container_from_handles(vector<GraphNodeHandle> handles, EditorNodeGraph& graph)
{
	SerializeGraphContainer container;
	// handles: nodes, also transitions, and nodes in sub layers
	while (!handles.empty()) {
		GraphNodeHandle h = handles.back();
		handles.pop_back();
		Base_EdNode* e = graph.get_node(h);
		if (!e) {
			LOG_WARN("no node");
			continue;
		}
		if (SetUtil::contains(container.nodes, e))
			continue;
		SetUtil::insert_test_exists(container.nodes, e);
		for (auto& l : e->links) {
			if (l.opt_link_node.is_valid()) {
				handles.push_back(l.opt_link_node);
			}
		}
		if (e->get_owning_sublayer().is_valid()) {
			NodeGraphLayer* layer = graph.get_layer(e->get_owning_sublayer());
			assert(layer);
			SetUtil::insert_test_exists(container.layers, layer);
			for (int i : layer->get_nodes()) {
				GraphNodeHandle handle(i);
				handles.push_back(handle);
			}
		}
	}
	return container;
}

SerializeGraphContainer SerializeGraphUtils::make_container_from_nodeids(const vector<int>& nodes, const vector<int>& links, EditorNodeGraph& graph)
{
	vector<GraphNodeHandle> handles;
	for (auto h : nodes)
		handles.push_back(GraphNodeHandle(h));
	for (auto l : links) {
		GraphPortHandle input;
		input.id = l;
		auto thenode = input.get_node_ptr(graph);
		if (thenode) {
			opt<int> idx = thenode->find_link_idx_from_port(input);
			if (idx.has_value()) {
				auto optnode = thenode->links.at(idx.value()).opt_link_node;
				if(optnode.is_valid())
					handles.push_back(optnode);
			}
		}
	}
	return make_container_from_handles(handles, graph);
}

Base_EdNode* AnimationGraphEditorNew::get_selected_node()
{
	vector<int> nodes;
	vector<int> links;
	GraphCommandUtil::get_selected(links, nodes);
	if (!nodes.empty() && !links.empty())
		return nullptr;
	if (nodes.size() == 1) {
		Base_EdNode* n = graph->get_node(nodes[0]);
		return n;
	}
	if (links.size() == 1) {
		Base_EdNode* n = GraphCommandUtil::get_optional_link_object(links[0], *graph);
		return n;
	}
	return nullptr;
}
void GraphPropertyWindow::draw()
{
	if (ImGui::Begin("Properties")) {
		grid.update();

		if (grid.rows_had_changes) {
			auto selected= ed.get_selected_node();
			if (selected)
				selected->on_property_changes();
			else
				ed.on_changed_graph_classes.invoke();
		}
	}
	ImGui::End();
}

extern ImFont* global_big_imgui_font;

static int count_characters(const std::string& s, char what) {
	int num = 0;
	for (char c : s)
		if (c == what)
			num++;
	return num;
}

void CommentNode::draw_imnode()
{
	ImNodes::PushColorStyle(ImNodesCol_NodeBackground, get_color().to_uint());

	const bool is_selected = editor->is_node_selected(*this);
	if (is_selected) {
		ImNodes::PushColorStyle(ImNodesCol_NodeOutline, GraphUtil::color32_to_int({ 255, 174, 0 }));
	}

	ImNodes::BeginComment(self.id);
	ImGui::PushFont(global_big_imgui_font);

	int lines = count_characters(desc, '\n') + 1;

	if (is_editing) {
		if (ImGui::InputTextMultiline("##text", (char*)desc.c_str(), desc.size() + 1, ImVec2(this->sizex-30.0, lines*30), ImGuiInputTextFlags_CallbackResize, imgui_std_string_resize,&desc)) {
		}
		if (ImGui::IsItemDeactivated()) {
			is_editing = false;
			desc = desc.c_str();
		}
	}
	else {
		ImVec2 pos = ImGui::GetCursorScreenPos();
		
		const float ofs = 2;
		ImGui::SetCursorScreenPos(ImVec2(pos.x + ofs, pos.y + ofs));
		ImGui::TextColored(ImColor(0,0,0),"%s", desc.c_str());
		ImGui::SetCursorScreenPos(pos);
		ImGui::Text("%s", desc.c_str());

		//ImGui::text
	}
	if (ImGui::IsItemHovered()) {
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			is_editing = true;
		}
	}

	ImGui::PopFont();
	ImNodes::EndComment();
	if (is_selected) {
		ImNodes::PopColorStyle();
	}
	ImNodes::PopColorStyle();

	auto pos = GraphUtil::to_glm(ImNodes::GetNodeGridSpacePos(self.id));
	auto size = GraphUtil::to_glm(ImNodes::GetCommentNodeSize(self.id));
	this->nodex = pos.x;
	this->nodey = pos.y;
	this->sizex = size.x;
	this->sizey = size.y;
}
GraphUtil::PinColorName GraphUtil::get_type_color_name(const GraphPinType::Enum& type)
{
	auto [color, shape] = get_pin_for_value_type(type);
	const char* str = "";
	switch (type)
	{
	case GraphPinType::Boolean: str = "Bool";break;
	case GraphPinType::Float: str = "Float";break;
	case GraphPinType::Integer:str = "Int";break;
	case GraphPinType::Vec3: str = "Vec3";break;
	case GraphPinType::Quat: str = "Quat";break;
	case GraphPinType::EnumType: str = "Enum";break;
	case GraphPinType::StateType: str = "State"; break;

	default:
		break;
	}
	return { color,str };
}
GraphUtil::PinShapeColor GraphUtil::get_pin_for_value_type(const GraphPinType::Enum& type)
{
	const auto circle = GraphUtil::PinShapeColor::Circle;
	const auto square = GraphUtil::PinShapeColor::Square;
	const auto tri = GraphUtil::PinShapeColor::Triangle;


	switch (type)
	{
	case GraphPinType::Any: return { {155,155,155},circle };
	case GraphPinType::Boolean: return { {170,30,30},circle };
	case GraphPinType::Float: return { {80,190,40},circle };
	case GraphPinType::Integer: return { {120,250,200},circle };
	case GraphPinType::Vec3: return { {230,130,50},circle };
	case GraphPinType::Quat: return { {120,120,250},circle };
	case GraphPinType::EnumType: return { {80,100,30},circle };
	case GraphPinType::StringName: return { {200,80,200},circle };
	case GraphPinType::ClassInfoType: return { {230,230,80},circle };
	case GraphPinType::LocalSpacePose: return { {220,220,220},square };
	case GraphPinType::MeshSpacePose: return { {90,180,220},square };

	case GraphPinType::StateType: return { {200,200,200},circle };

	default: return {};

	}

}

Color32 CommentNode::get_color() const
{
	switch (color)
	{
	case CommentColors::Gray:return { 100,100,100 };
	case CommentColors::Red: return { 120,10,10 };
	case CommentColors::Green: return { 50,90,30 };
	case CommentColors::Blue: return { 20,60,120 };
	case CommentColors::Yellow: return { 120,120,20 };
	case CommentColors::Orange: return { 120,70,20 };
	default:
		break;
	}
	return { 100,100,100 };
}
void AnimationGraphEditorNew::resolve_any_types()
{
	auto recurse = [](auto&& self,unordered_set<int>& visited, EditorNodeGraph& graph, Base_EdNode* n) {
		if (!n)
			return;
		if (visited.find(n->self.id) != visited.end())
			return;
		visited.insert(n->self.id);
		vector<int> inputs, outputs;
		n->get_ports(inputs, outputs);
		for (int inputidx : inputs) {
			GraphPort* myport = &n->ports.at(inputidx);
			Base_EdNode* other = n->find_other_node_from_port(myport->get_handle(n->self));
			self(self, visited, graph, other);
		}

		ImNodes::EditorContextSet(graph.get_layer(n->layer)->get_context());
		n->on_link_changes();
	};

	unordered_set<int> visited;
	const auto& nodes = graph->get_nodes();
	for (auto n : nodes) {
		// go down the chain, then go back up
		recurse(recurse, visited, *graph, n);
	}
}
ControlParamsWindowNew::ControlParamsWindowNew(AnimationGraphEditorNew& ed)
	:ed(ed)
{
	ed.on_changed_graph_classes.add(this, [this]() {
		refresh_props();
		});
	refresh_props();
}
void ControlParamsWindowNew::refresh_props()
{
	props.clear();
	ed.get_var_prototypes().creations.clear();
	opt<int> idx = ed.get_menu().find_item("Variables");
	assert(idx.has_value());
	NodeMenu& menu = ed.get_menu().menus.at(idx.value()).menu.value();
	menu.menus.clear();
	auto animclass = ed.get_options().anim_class_type.ptr;
	if (!animclass)
		return;
	for (PropertyPtr p : ClassPropPtr(animclass)) {
		VariableParam param;
		param.nativepi = p.get_property_info();
		if (p.is_boolean()) {
			param.type = GraphPinType::Boolean;
		}
		else if (p.is_enum()) {
			param.type = GraphPinType::EnumType;
			param.type.data = p.get_property_info()->enum_type;
		}
		else if (p.is_float()) {
			param.type = GraphPinType::Float;
		}
		else if (p.is_numeric()) {
			param.type = GraphPinType::Integer;
		}
		else if (p.is_vec3()) {
			param.type = GraphPinType::Vec3;
		}
		else if (p.is_quat()) {
			param.type = GraphPinType::Quat;
		}
		else {
			continue;
		}
		props.push_back(param);
	}
	for (auto& p : props) {
		auto [color, str] = GraphUtil::get_type_color_name(p.type.type);
		string name_as_str = p.nativepi->name;
		GraphPinType the_type = p.type;
		menu.add(name_as_str, color);
		ed.get_var_prototypes().add(name_as_str, [name_as_str,the_type]() { return new Variable_EdNode(name_as_str, the_type); });
	}

	ed.on_node_changes.invoke();
}
void ControlParamsWindowNew::imgui_draw()
{
	if (!ImGui::Begin("Instance Parameters")) {
		ImGui::End();
		return;
	}

	uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;

	const bool is_graph_running = false;// anim_graph_ed.playback != AnimationGraphEditor::graph_playback_state::stopped;

	const int num_cols = (is_graph_running) ? 3 : 2;

	if (ImGui::BeginTable("controlproplistabc", num_cols, ent_list_flags))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
		if (is_graph_running)
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 50.0f, 0);
		ImGui::TableHeadersRow();

		for (int row_n = 0; row_n < props.size(); row_n++)
		{
			auto& res = props[row_n];
			ImGui::PushID(res.nativepi->name);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
			if (ImGui::Selectable("##selectednode", false, selectable_flags, ImVec2(0, 0))) {

			}

			//if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			//{
			//	dragdrop = { res.str, res.type };
			//	auto ptr = &dragdrop;
			//
			//
			//	ImGui::SetDragDropPayload("AnimGraphVariableDrag", &ptr, sizeof(VariableNameAndType*));
			//
			//	ImGui::TextColored(scriptparamtype_to_color(res.type), res.str.c_str());
			//
			//	ImGui::EndDragDropSource();
			//}

			ImGui::SameLine();
			ImGui::Text("%s",res.nativepi->name);

#if 0
			if (is_graph_running) {
				ImGui::TableNextColumn();
				switch (res.type)
				{
				case anim_graph_value::float_t: {
					float f = res.nativepi->get_float(anim_graph_ed.out.get_animator());
					ImGui::DragFloat("##inpf", &f, 0.05);
					res.nativepi->set_float(anim_graph_ed.out.get_animator(), f);
				}break;
				case anim_graph_value::bool_t: {
					bool b = res.nativepi->get_int(anim_graph_ed.out.get_animator());
					ImGui::Checkbox("##inpf", &b);
					res.nativepi->set_int(anim_graph_ed.out.get_animator(), b);
				}break;
				case anim_graph_value::vec3_t: {
					glm::vec3* v = (glm::vec3*)res.nativepi->get_ptr(anim_graph_ed.out.get_animator());
					ImGui::DragFloat3("##inpf", &v->x, 0.025);
				}break;
				case anim_graph_value::quat_t: {
					glm::quat* v = (glm::quat*)res.nativepi->get_ptr(anim_graph_ed.out.get_animator());

					glm::vec3 eul = glm::eulerAngles(*v);
					eul *= 180.f / PI;
					if (ImGui::DragFloat3("##eul", &eul.x, 1.0)) {
						eul *= PI / 180.f;
						*v = glm::quat(eul);
					}

				}break;
				case anim_graph_value::int_t:
				{
					int b = res.nativepi->get_int(anim_graph_ed.out.get_animator());
					ImGui::InputInt("##inpf", &b);
					res.nativepi->set_int(anim_graph_ed.out.get_animator(), b);
				}break;
				};
			}
#endif


			ImGui::TableNextColumn();
			//const EnumIntPair* eip = EnumTrait<anim_graph_value>::StaticEnumType.find_for_value((int)res.type);
			//if (!eip) {
			//	printf("Warning: anim_graph_value_t bad\n");
			//	res.type = anim_graph_value::bool_t;
			//	eip = EnumTrait<anim_graph_value>::StaticEnumType.find_for_value((int)res.type);
			//	ASSERT(eip);
			//}
			auto [color, str] = GraphUtil::get_type_color_name(res.type.type);
			ImGui::TextColored(ImColor(color.to_uint()),"%s",str);

			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	ImGui::End();
}
Color32 get_color_for_category(EdNodeCategory cat)
{
	Color32 out = { 40,40,40 };
	switch (cat)
	{
	case EdNodeCategory::Math: out={ 22, 61, 99 };
		break;
	case EdNodeCategory::Function: out={ 94, 31, 31 };
		break;
	case EdNodeCategory::AnimSource: out={ 40,40,40 };
		break;
	case EdNodeCategory::AnimBlend: out={ 13, 82, 23 };
		break;
	case EdNodeCategory::AnimBoneModify: out={ 138, 109, 17 };
		break;
	default:
		break;
	}
	return GraphUtil::add_brightness(out, 90);
}
NodeGraphLayer::TabNameAndBackground NodeGraphLayer::get_tab_name(EditorNodeGraph& graph)
{
	Color32 defualtcolor = { 50,50,50 };
	if (!owner.is_valid()) {
		return { "RootLayer",defualtcolor };
	}
	Base_EdNode* owner = graph.get_node(this->owner);
	if (!owner) {
		return { "NoOwner?",defualtcolor };
	}
	Color32 color = defualtcolor;
	if (type==Statemachine)
		color = { 20,10,0 };
	else if (type == Transition)
		color = { 0,10,40 };
	return { owner->get_layer_title(),color };
}

void AnimationGraphEditorNew::draw_layer_window()
{
	auto draw_layers_R = [](auto&& self, AnimationGraphEditorNew& editor,NodeGraphLayer* layer,  int tab) ->void {
		EditorNodeGraph& graph = editor.get_graph();
		auto& nodes = layer->get_nodes();
		opt<GraphLayerHandle> handle = editor.tab_manager->get_active_tab();
		const bool is_selected = handle.has_value() && handle.value() == layer->get_id();
		auto[name,color] = layer->get_tab_name(graph);
		color.r = 255 - color.r;
		color.g = 255 - color.g;
		color.b = 255 - color.b;


		ImColor imcol = color.to_uint();


		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		//ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, imcol);

		const float tabwidth = 5.f;
		//ImGui::Dummy(ImVec2(tab * tabwidth, 0.f));
		//ImGui::SameLine();
		ImGui::PushID(layer);
		ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
		if (ImGui::Selectable("##selectednode", is_selected, selectable_flags, ImVec2(0, 0))) {
			editor.tab_manager->open_tab(layer->get_id(), true);
		}
		ImGui::PopID();
		ImGui::SameLine();
		string s;
		for (int i = 0; i < tab; i++) s += ".";

		ImGui::PushStyleColor(ImGuiCol_Text, ImU32(imcol));
		ImGui::Text("%s", (s+name).c_str());
		ImGui::PopStyleColor();

		for (int i : nodes) {
			GraphNodeHandle h(i);
			Base_EdNode* e = graph.get_node(h);
			if (e->get_owning_sublayer().is_valid()) {
				NodeGraphLayer* sub = graph.get_layer(e->get_owning_sublayer());
				if (sub) {
					self(self, editor, sub, tab + 1);
				}
			}
		}

	};

	if (ImGui::Begin("Layers")) {
		ImGuiTableFlags const flags =  ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;

		if (ImGui::BeginTable("Table", 1, flags)) {
			ImGui::TableSetupColumn("##Editor", ImGuiTableColumnFlags_WidthStretch);
			draw_layers_R(draw_layers_R,*this, graph->get_root(), 0);
			ImGui::EndTable();
		}

	}
	ImGui::End();
}