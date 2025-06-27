#define IMGUI_DEFINE_MATH_OPERATORS
#include "PropertyEditors.h"
#include "Framework/FnFactory.h"
#include "Assets/AssetRegistry.h"
#include "Framework/Config.h"
#include "Assets/AssetBrowser.h"
#include "Game/SoftAssetPtr.h"
#include "LevelEditor/EditorDocLocal.h"
#include "Assets/AssetDatabase.h"
#include "Framework/MyImguiLib.h"

#include "Game/Components/PhysicsComponents.h"
#include "Game/Components/LightComponents.h"

#include "Animation/Editor/AnimationGraphEditor.h"

#include "Animation/Editor/Blendspace_nodes.h"
#include "Render/Editor/MaterialEditorLocal.h"
#include "Animation/Runtime/Statemachine_cfg.h"

#include "imgui_internal.h"

bool SharedAssetPropertyEditor::internal_update() {
	if (!has_init) {
		has_init = true;
		asset_str = get_str();
		metadata = AssetRegistrySystem::get().find_for_classtype(prop->class_type);
	}
	if (!metadata) {
		ImGui::Text("Asset has no metadata: %s\n", prop->class_type->classname);
		return false;
	}


	auto drawlist = ImGui::GetWindowDrawList();
	auto& style = ImGui::GetStyle();
	auto min = ImGui::GetCursorScreenPos();
	auto sz = ImGui::CalcTextSize(asset_str.c_str());
	float width = ImGui::CalcItemWidth();
	Color32 color = metadata->get_browser_color();
	color.r *= 0.4;
	color.g *= 0.4;
	color.b *= 0.4;

	if (is_soft_editor()) {
		float border = 2.f;
		drawlist->AddRectFilled(
			ImVec2(min.x - style.FramePadding.x * 0.5f - border, min.y - border),
			ImVec2(min.x + width + border, min.y + sz.y + style.FramePadding.y * 2.0 + border),
			(Color32{ 255, 229, 99 }).to_uint());
	}

	drawlist->AddRectFilled(ImVec2(min.x - style.FramePadding.x * 0.5f, min.y), ImVec2(min.x + width, min.y + sz.y + style.FramePadding.y * 2.0),
		color.to_uint());
	auto cursor = ImGui::GetCursorPos();

	if (is_soft_editor())
		ImGui::TextColored(ImColor((Color32{ 255, 229, 99 }).to_uint()), asset_str.c_str());
	else {
		if (get_failed_load())
			ImGui::TextColored(ImColor((Color32{ 255, 141, 133 }).to_uint()), asset_str.c_str());
		else
			ImGui::Text(asset_str.c_str());
	}
	ImGui::SetCursorPos(cursor);
	ImGui::InvisibleButton("##adfad", ImVec2(width, sz.y + style.FramePadding.y * 2.f));
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
		ImGui::BeginTooltip();
		if (is_soft_editor())
			ImGui::Text(string_format("SoftAssetPtr: Drag and drop %s asset here", metadata->get_type_name().c_str()));
		else
			ImGui::Text(string_format("Drag and drop %s asset here", metadata->get_type_name().c_str()));
		ImGui::EndTooltip();


		if (ImGui::GetIO().MouseDoubleClicked[0]) {
			if (metadata->tool_to_edit_me()) {
				std::string cmdstr = "start_ed ";
				cmdstr += '"';
				cmdstr += metadata->get_type_name();
				cmdstr += '"';
				cmdstr += " ";
				cmdstr += '"';
				cmdstr += asset_str.c_str();
				cmdstr += '"';
				Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, cmdstr.c_str());
			}
		}
		else if (ImGui::GetIO().MouseClicked[0]) {
			AssetBrowser::inst->filter_all();
			AssetBrowser::inst->unset_filter(1 << metadata->self_index);
		}
	}
	bool ret = false;
	if (ImGui::BeginDragDropTarget())
	{
		//const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		//if (payload->IsDataType("AssetBrowserDragDrop"))
		//	sys_print("``` accepting\n");

		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop", ImGuiDragDropFlags_AcceptPeekOnly);
		if (payload) {

			AssetOnDisk* resource = *(AssetOnDisk**)payload->Data;
			bool actually_accept = false;
			if (resource->type == metadata) {
				actually_accept = true;
			}

			if (actually_accept) {
				if (payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop"))
				{
					//IAsset** ptr_to_asset = (IAsset**)prop->get_ptr(instance);

					set_asset(resource->filename);
					asset_str = get_str();


					ret = true;
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	return ret;

}

std::string SoftAssetPropertyEditor::get_str() {
	auto ptr = (SoftAssetPtr<IAsset>*)prop->get_ptr(instance);
	return ptr->path;
}

void SoftAssetPropertyEditor::set_asset(const std::string& str) {
	auto ptr = (SoftAssetPtr<IAsset>*)prop->get_ptr(instance);
	ptr->path = str;
}

std::string AssetPropertyEditor::get_str() {
	auto ptr = (IAsset**)prop->get_ptr(instance);
	return (*ptr) ? (*ptr)->get_name() : "";
}

void AssetPropertyEditor::set_asset(const std::string& str) {
	auto ptr = (IAsset**)prop->get_ptr(instance);
	if (str.empty()) {
		*ptr = nullptr;
	}
	else {
		auto classtype = prop->class_type;
		auto asset = g_assets.find_sync(str, classtype, 0).get();// loader->load_asset(resource->filename);
		*ptr = asset;
	}
}

bool AssetPropertyEditor::get_failed_load() const {
	auto ptr = *(IAsset**)prop->get_ptr(instance);
	if (ptr && ptr->did_load_fail())
		return true;
	return false;
}

EntityPtrAssetEditor::EntityPtrAssetEditor(EditorDoc& editor) :editor(editor) {
	editor.on_eyedropper_callback.add(this, [&](const Entity* e)
		{
			if (editor.get_active_eyedropper_user_id() == this) {
				sys_print(Debug, "entityptr on eye dropper callback\n");
				EntityPtr* ptr_to_asset = (EntityPtr*)prop->get_ptr(instance);
				*ptr_to_asset = e->get_self_ptr();
			}
		});
}

EntityPtrAssetEditor::~EntityPtrAssetEditor() {
	editor.on_eyedropper_callback.remove(this);
}

bool EntityPtrAssetEditor::internal_update() {

	EntityPtr* ptr_to_asset = (EntityPtr*)prop->get_ptr(instance);

	ImGui::PushStyleColor(ImGuiCol_Button, color32_to_imvec4({ 51, 10, 74,200 }));
	auto eyedropper = g_assets.find_global_sync<Texture>("icon/eyedrop.png");
	if (ImGui::ImageButton((ImTextureID)uint64_t(eyedropper->gl_id), ImVec2(16, 16))) {
		editor.enable_entity_eyedropper_mode(this);
	}
	ImGui::PopStyleColor();
	ImGui::SameLine();
	if (editor.is_in_eyedropper_mode() && editor.get_active_eyedropper_user_id() == this) {
		ImGui::TextColored(color32_to_imvec4({ 255, 74, 249 }), "{ eyedropper  active }");
	}
	else if (ptr_to_asset->get()) {
		const char* str = ptr_to_asset->get()->get_editor_name().c_str();
		if (!*str)
			str = ptr_to_asset->get()->get_type().classname;

		std::string what = str;
		what += "  id:(" + std::to_string(ptr_to_asset->get()->get_instance_id()) + ")";

		ImGui::Text("%s", what.c_str());
	}
	else {
		ImGui::TextColored(color32_to_imvec4({ 128,128,128 }), "<nullptr>");

	}

	return false;
}

bool ColorEditor::internal_update() {
	assert(prop->type == core_type_id::Int32);
	Color32* c = (Color32*)prop->get_ptr(instance);
	ImVec4 col = ImGui::ColorConvertU32ToFloat4(c->to_uint());
	if (ImGui::ColorEdit3("##coloredit", &col.x)) {
		auto uint_col = ImGui::ColorConvertFloat4ToU32(col);
		uint32_t* prop_int = (uint32_t*)prop->get_ptr(instance);
		*prop_int = uint_col;
		return true;
	}
	return false;
}

int ColorEditor::extra_row_count() { return 0; }

bool ColorEditor::can_reset() {
	Color32* c = (Color32*)prop->get_ptr(instance);
	return c->r != 255 || c->g != 255 || c->b != 255;

}

void ColorEditor::reset_value() {
	Color32* c = (Color32*)prop->get_ptr(instance);
	*c = COLOR_WHITE;
}

bool ButtonPropertyEditor::internal_update() {
	ASSERT(prop->type == core_type_id::ActualStruct && prop->struct_type == &BoolButton::StructType);
	BoolButton* b = (BoolButton*)prop->get_ptr(instance);

	bool ret = false;
	if (ImGui::Button(prop->tooltip)) {
		ret = true;
		b->b = true;
	}

	return ret;
}

bool ButtonPropertyEditor::can_reset() {
	return false;
}

AnchorJointEditor::~AnchorJointEditor() {
	if (editor.manipulate->is_using_key_for_custom(this))
		editor.manipulate->stop_using_custom();
}

// Inherited via IPropertyEditor

bool AnchorJointEditor::internal_update()
{
	JointAnchor* j = (JointAnchor*)prop->get_ptr(instance);

	Entity* me = editor.selection_state->get_only_one_selected().get();
	if (!me) {
		ImGui::Text("no Entity* found\n");
		return false;
	}

	if (editor.manipulate->is_using_key_for_custom(this)) {
		auto last_matrix = editor.manipulate->get_custom_transform();
		auto local = glm::inverse(me->get_ws_transform()) * last_matrix;
		j->q = (glm::quat_cast(local));
		j->p = local[3];

	};

	bool ret = false;
	if (ImGui::DragFloat3("##vec", (float*)&j->p, 0.05))
		ret = true;
	glm::vec3 eul = glm::eulerAngles(j->q);
	eul *= 180.f / PI;
	if (ImGui::DragFloat3("##eul", &eul.x, 1.0)) {
		eul *= PI / 180.f;
		j->q = glm::normalize(glm::quat(eul));

		ret = true;
	}

	glm::mat4 matrix = glm::translate(glm::mat4(1.f), j->p) * glm::mat4_cast(j->q);
	editor.manipulate->set_start_using_custom(this, me->get_ws_transform() * matrix);

	return true;
}

CubemapAnchorEditor::~CubemapAnchorEditor() {
	if (editor.manipulate->is_using_key_for_custom(this))
		editor.manipulate->stop_using_custom();
}

// Inherited via IPropertyEditor

bool CubemapAnchorEditor::internal_update()
{
	CubemapAnchor* j = (CubemapAnchor*)prop->get_ptr(instance);
	Entity* me = editor.selection_state->get_only_one_selected().get();
	if (!me) {
		ImGui::Text("no Entity* found\n");
		return false;
	}

	ImGui::Checkbox("edit_anchor", &using_this);

	if (!using_this) {
		editor.manipulate->stop_using_custom();
	}

	if (using_this) {
		if (editor.manipulate->is_using_key_for_custom(this)) {
			auto last_matrix = editor.manipulate->get_custom_transform();
			auto local = glm::inverse(me->get_ws_transform()) * last_matrix;
			j->p = local[3];
		};
	}

	bool ret = false;
	if (ImGui::DragFloat3("##vec", (float*)&j->p, 0.05))
		ret = true;

	if (using_this) {
		glm::mat4 matrix = glm::translate(glm::mat4(1.f), j->p);
		editor.manipulate->set_start_using_custom(this, me->get_ws_transform() * matrix);

		return true;
	}

	return ret;

}
/*


		pfac.registerClass<FindAnimationClipPropertyEditor>("AG_CLIP_TYPE");
		pfac.registerClass<FindAnimGraphVariableProp>("FindAnimGraphVariableProp");
		pfac.registerClass<AgBoneFinder>("AgBoneFinder");

		pfac.registerClass<BlendspaceGridEd>("BlendspaceGridEd");

		pfac.registerClass<AgEnumFinder>("AG_ENUM_TYPE_FINDER");

*/

bool BlendspaceGridEd::internal_update()
{
#if 0
	//auto str = (VariableNameAndType*)prop->get_ptr(instance);
	//drag_drop_property_ed_func(&str->str, Color32{ 62, 27, 82 }, [&](void* payload) {
	//
	//	VariableNameAndType* resource = *(VariableNameAndType**)payload;
	//	*str = *resource;
	//
	//	}, "AnimGraphVariableDrag", "variable");
	Blendspace2d_EdNode* node = (Blendspace2d_EdNode*)instance;
	if (node->cols == 0 || node->rows == 0)	// only happens on startup
		node->resize_grid(1, 1);

	auto drawlist = ImGui::GetWindowDrawList();
	auto& style = ImGui::GetStyle();


	const Color32 unusedc = { 42, 48, 39 };
	const Color32 usedc = { 124, 150, 111 };
	const Color32 hoveredc = { 187, 196, 61 };
	const float square_width = 48;

	static int x_popup_idx = 0;
	static int y_popup_idx = 0;


	for (int y = 0; y < node->rows; y++) {
		for (int x = 0; x < node->cols; x++) {

			auto min = ImGui::GetCursorScreenPos();

			const int index = y * node->cols + x;
			auto& gridpoint = node->gridpoints[index];
			const bool hovered = ImRect(min, min + ImVec2(square_width, square_width)).Contains(ImGui::GetMousePos());
			Color32 color_to_use = unusedc;
			if (hovered) color_to_use = hoveredc;
			else if (gridpoint.animation) color_to_use = usedc;

			drawlist->AddRectFilled(ImVec2(min.x, min.y), ImVec2(min.x + square_width, min.y + square_width), color_to_use.to_uint());

			ImGui::PushClipRect(min, min + ImVec2(square_width, square_width), false);
			if (gridpoint.animation)
				drawlist->AddText(NULL, 0.0f, min, COLOR_WHITE.to_uint(), gridpoint.animation->get_name().c_str(), nullptr, square_width);
			ImGui::PopClipRect();

			ImGui::InvisibleButton("##adfad", ImVec2(square_width, square_width));
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
				if (gridpoint.animation) {
					ImGui::BeginTooltip();
					ImGui::Text(gridpoint.animation->get_name().c_str());
					ImGui::EndTooltip();
				}
				if (ImGui::GetIO().MouseClicked[1]) {
					ImGui::OpenPopup("blendspace_grid_popup");
					x_popup_idx = x;
					y_popup_idx = y;
				}
			}
			if (ImGui::BeginDragDropTarget())
			{
				//const ImGuiPayload* payload = ImGui::GetDragDropPayload();
				//if (payload->IsDataType("AssetBrowserDragDrop"))
				//	sys_print("``` accepting\n");

				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop"))
				{
					AssetOnDisk* aod = *(AssetOnDisk**)payload->Data;
					if (aod->type->get_asset_class_type()->is_a(AnimationSeqAsset::StaticType)) {

						gridpoint.animation = g_assets.find_sync<AnimationSeqAsset>(aod->filename);
					}

				}
				ImGui::EndDragDropTarget();
			}

			if (x != node->cols - 1)
				ImGui::SameLine();
		}
	}

	if (ImGui::BeginPopup("blendspace_grid_popup")) {

		bool close_popup = false;
		auto& gridpoint = node->gridpoints[y_popup_idx * node->cols + x_popup_idx];
		if (gridpoint.animation && ImGui::SmallButton("remove animation")) {
			gridpoint.animation.ptr = nullptr;
			close_popup = true;
		}
		ImGui::Separator();

		int gridpoints_before = node->gridpoints.size();

		if (ImGui::SmallButton("add row above"))
			node->append_row_to_grid(y_popup_idx);
		if (ImGui::SmallButton("add row below"))
			node->append_row_to_grid(y_popup_idx + 1);
		if (node->rows > 1 && ImGui::SmallButton("remove this row"))
			node->remove_row_from_grid(y_popup_idx);

		ImGui::Separator();

		if (ImGui::SmallButton("add col left"))
			node->append_col_to_grid(x_popup_idx);
		if (ImGui::SmallButton("add col right"))
			node->append_col_to_grid(x_popup_idx + 1);
		if (node->cols > 1 && ImGui::SmallButton("remove this col"))
			node->remove_col_from_grid(x_popup_idx);


		int gridpoints_after = node->gridpoints.size();
		if (gridpoints_after != gridpoints_before || close_popup)
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
#endif

	return false;
}

AgBoneFinder::AgBoneFinder(AnimationGraphEditor& editor) : editor(editor)
{
#if 0
	auto model = editor.out.get_model();
	if (!model || !model->get_skel()) {
		no_model = true;
		return;
	}
	for (int i = 0; i < model->get_skel()->bone_dat.size(); i++)
		bones.push_back(model->get_skel()->bone_dat[i].strname);
#endif

}

// Inherited via IPropertyEditor

bool AgBoneFinder::internal_update()
{
	ASSERT(prop->type == core_type_id::StdString);
	std::string* str = (std::string*)prop->get_ptr(instance);
	bool ret = false;
	if (no_model)
		ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1.0), "no model set");
	else if (bones.empty())
		ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1.0), "model has no bones");
	else {

		if (ImGui::BeginCombo("##bones", str->c_str())) {
			for (int i = 0; i < bones.size(); i++) {
				bool is_selected = *str == bones[i];
				if (ImGui::Selectable(bones[i].c_str(), &is_selected)) {
					*str = bones[i];
					ret = true;
				}
			}
			ImGui::EndCombo();
		}

	}
	return ret;
}

// Inherited via IPropertyEditor


template<typename FUNCTOR>
static bool drag_drop_property_ed_func(std::string* str, Color32 color, FUNCTOR&& callback, const char* targetname, const char* tooltip)
{

	//ImGui::PushStyleColor(Imguicolba)

//	ImGui::InputText("##inp", (char*)str->c_str(), str->size(), ImGuiInputTextFlags_ReadOnly);

	//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // Ensure no clip rect so mouse hover can reach FramePadding edges
	auto drawlist = ImGui::GetWindowDrawList();
	auto& style = ImGui::GetStyle();
	auto min = ImGui::GetCursorScreenPos();
	auto sz = ImGui::CalcTextSize(str->c_str());
	float width = ImGui::CalcItemWidth();
	drawlist->AddRectFilled(ImVec2(min.x - style.FramePadding.x * 0.5f, min.y), ImVec2(min.x + width, min.y + sz.y + style.FramePadding.y * 2.0), color.to_uint());
	auto cursor = ImGui::GetCursorPos();
	ImGui::Text(str->c_str());
	ImGui::SetCursorPos(cursor);
	ImGui::InvisibleButton("##adfad", ImVec2(width, sz.y + style.FramePadding.y * 2.f));
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
		ImGui::BeginTooltip();
		ImGui::Text(string_format("Drag and drop %s asset here", tooltip));
		ImGui::EndTooltip();
	}
	bool return_val = false;
	if (ImGui::BeginDragDropTarget())
	{
		//const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		//if (payload->IsDataType("AssetBrowserDragDrop"))
		//	sys_print("``` accepting\n");

		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(targetname))
		{
			return_val = callback(payload->Data);

		}
		ImGui::EndDragDropTarget();
	}
	return return_val;

}

bool FindAnimationClipPropertyEditor::internal_update()
{
	ASSERT(prop->type == core_type_id::StdString);

	auto str = (std::string*)prop->get_ptr(instance);
	return drag_drop_property_ed_func(str, Color32{ 39, 56, 35 }, [&](void* payload) {

		std::string* resource = *(std::string**)payload;
		*str = *resource;

		return true;

		}, "AnimationItemAnimGraphEd", "animation");

}

bool FindAnimGraphVariableProp::internal_update()
{
#if 0
	auto str = (VariableNameAndType*)prop->get_ptr(instance);
	return drag_drop_property_ed_func(&str->str, Color32{ 62, 27, 82 }, [&](void* payload) {

		VariableNameAndType* resource = *(VariableNameAndType**)payload;
		*str = *resource;

		return true;

		}, "AnimGraphVariableDrag", "variable");
#endif
	return false;

}
int imgui_std_string_resize(ImGuiInputTextCallbackData* data);
bool CodeBlockPropEditor::internal_update()
{
	auto& str = *(std::string*)prop->get_ptr(instance);
	ImGui::InputTextMultiline("##input", str.data(), str.size() + 1, ImVec2(0, 0), ImGuiInputTextFlags_CallbackResize, imgui_std_string_resize,&str);
	return false;
}

void PropertyFactoryUtil::register_basic(FnFactory<IPropertyEditor>& factory)
{
	factory.add("BoolButton", []() {return new ButtonPropertyEditor; });
	factory.add("ColorUint", []() {return new ColorEditor; });
	factory.add("AssetPtr", []() {return new AssetPropertyEditor; });
	factory.add("SoftAssetPtr", []() {return new SoftAssetPropertyEditor; });
	factory.add("ClassTypePtr", []() {return new ClassTypePtrPropertyEditor; });

	factory.add("code_block", []() {return new CodeBlockPropEditor; });

}
void PropertyFactoryUtil::register_editor(EditorDoc& doc, FnFactory<IPropertyEditor>& factory)
{
	factory.add("ObjPtr", [&doc]() {return new EntityPtrAssetEditor(doc); });
	factory.add("JointAnchor", [&doc]() {return new AnchorJointEditor(doc); });
	factory.add("CubemapAnchor", [&doc]() {return new CubemapAnchorEditor(doc); });

	factory.add("EntityTagString", []() {return new EntityTagEditor; });
	factory.add("EntityBoneParentString", []() {return new EntityBoneParentStringEditor; });
}
void PropertyFactoryUtil::register_anim_editor(AnimationGraphEditor& doc, FnFactory<IPropertyEditor>& factory)
{
	factory.add("AG_CLIP_TYPE", [&doc]() {return new FindAnimationClipPropertyEditor(doc); });
	factory.add("FindAnimGraphVariableProp", [&doc]() {return new FindAnimGraphVariableProp(doc); });
	factory.add("AgBoneFinder", [&doc]() {return new AgBoneFinder(doc); });
	factory.add("BlendspaceGridEd", [&doc]() {return new BlendspaceGridEd(doc); });
}
void PropertyFactoryUtil::register_mat_editor(MaterialEditorLocal& doc, FnFactory<IPropertyEditor>& factory)
{
	factory.add("MaterialEditParam", [&doc]() {return new MaterialParamPropEditor(doc); });
}
#include "Framework/PropertyPtr.h"
#include "Animation/Editor/ClipNode.h"
#include "Animation/Editor/AnimationGraphEditor2.h"
class StatemachineAliasEditor : public IPropertyEditor
{
public:
	StatemachineAliasEditor(AnimationGraphEditorNew& ed) :ed(ed) {

	}
	bool internal_update() final {
		PropertyPtr ptr(prop, instance);
		StateAliasStruct* st = ptr.as_struct().get_struct<StateAliasStruct>();

		if (ImGui::Checkbox("##defaulttrue", &st->default_true)) {
			for (auto& h : st->handles)
				h.flag = st->default_true;
		}
		ImGui::SameLine();
		ImGui::Text("DefaultTrue");

		ImGui::Separator();

		assert(st);
		for (int i = 0; i < st->handles.size(); i++) {
			auto& s = st->handles[i];

			auto node = ed.get_graph().get_node(s.handle);
			if (!node||!node->is_a<State_EdNode>())
				continue;
			
			ImGui::PushID(node);
			(ImGui::Checkbox("##box", &s.flag));
			ImGui::PopID();

			ImGui::SameLine();
			auto asstate = node->cast_to<State_EdNode>();
			assert(asstate);
			ImGui::Text("%s", asstate->get_layer_title().c_str());
		}
		return false;
	}

	AnimationGraphEditorNew& ed;
};

void PropertyFactoryUtil::register_anim_editor2(AnimationGraphEditorNew& ed, FnFactory<IPropertyEditor>& factory)
{
	factory.add("StateAliasStruct", [&ed]() {return new StatemachineAliasEditor(ed); });
}

// Inherited via IPropertyEditor

bool MaterialParamPropEditor::internal_update()
{
	if (!has_init) {
		auto& factory = editor.factory;

		const int index = prop->offset;
		MaterialEditorLocal* mLocal = &editor;
		auto& paramDef = mLocal->dynamicMat->get_master_material()->param_defs.at(index);
		MaterialInstance* mInstLocal = (MaterialInstance*)mLocal->dynamicMat.get();
		auto& param = mInstLocal->impl->params.at(index);
		pi.name = paramDef.name.c_str();
		pi.offset = offsetof(MaterialParameterValue, tex_ptr);
		if (param.type == MatParamType::Bool) {
			pi.type = core_type_id::Bool;
			internalEditor = std::make_unique<BooleanEditor>(&param, &pi);
		}
		else if (param.type == MatParamType::Float) {
			pi.type = core_type_id::Float;
			internalEditor = std::make_unique<FloatEditor>(&param, &pi);
		}
		else if (param.type == MatParamType::Vector) {
			pi.type = core_type_id::Int32;
			auto ed = factory.create("ColorUint");
			assert(ed);
			internalEditor = std::unique_ptr<IPropertyEditor>(ed);
			internalEditor->post_construct_for_custom_type(&param, &pi, nullptr);
		}
		else if (param.type == MatParamType::Texture2D) {
			pi = make_assetptr_property_new(paramDef.name.c_str(), pi.offset, PROP_DEFAULT, "", &Texture::StaticType);// (AssetPtr<Texture>*)(0));
			auto ed = factory.create(pi.custom_type_str/*AssetPtr*/);
			assert(ed);
			internalEditor = std::unique_ptr<IPropertyEditor>(ed);
			internalEditor->post_construct_for_custom_type(&param, &pi, nullptr);
		}
		has_init = true;
	}
	if (!internalEditor)
		return false;
	return internalEditor->internal_update();
}

// Inherited via IPropertyEditor


#include "LevelEditor/TagManager.h"


EntityTagEditor::~EntityTagEditor() {
	StringName* myName = (StringName*)prop->get_ptr(instance);
	*myName = StringName(str.c_str());
}

// Inherited via IPropertyEditor

bool EntityTagEditor::internal_update()
{
	Entity* self = (Entity*)instance;
	if (!has_init) {

		auto& all_tags = GameTagManager::get().registered_tags;
		for (auto& t : all_tags)
			options.push_back(t);

		has_init = true;
		StringName* myName = (StringName*)prop->get_ptr(instance);
		if (!myName->is_null())
			str = myName->get_c_str();
	}

	if (options.empty()) {
		ImGui::Text("No options, add tag in init.txt with REG_GAME_TAG <tag>");
		return false;
	}

	bool has_update = false;


	const char* preview = (!str.empty()) ? str.c_str() : "<untagged>";
	if (ImGui::BeginCombo("##combocalsstype", preview)) {
		for (auto& option : options) {

			if (ImGui::Selectable(option.c_str(),
				str == option
			)) {
				self->set_tag(StringName(option.c_str()));
				str = option;
				has_update = true;
			}

		}
		ImGui::EndCombo();
	}

	return has_update;
}

// Inherited via IPropertyEditor

EntityBoneParentStringEditor::~EntityBoneParentStringEditor() {
	StringName* myName = (StringName*)prop->get_ptr(instance);
	*myName = StringName(str.c_str());
}

bool EntityBoneParentStringEditor::internal_update()
{
	Entity* self = (Entity*)instance;
	if (!has_init) {
		Entity* parent = self->get_parent();
		if (parent) {
			MeshComponent* mc = parent->get_component<MeshComponent>();
			if (mc && mc->get_model() && mc->get_model()->get_skel()) {
				const Model* mod = mc->get_model();
				auto skel = mod->get_skel();
				auto& allbones = skel->get_all_bones();
				for (auto& b : allbones) {
					options.push_back(b.strname);
				}
			}
		}
		has_init = true;
		StringName* myName = (StringName*)prop->get_ptr(instance);
		if (!myName->is_null())
			str = myName->get_c_str();
	}

	if (options.empty()) {
		ImGui::Text("No options (add a MeshComponent with a skeleton)");
		return false;
	}

	bool has_update = false;


	const char* preview = (!str.empty()) ? str.c_str() : "<empty>";
	if (ImGui::BeginCombo("##combocalsstype", preview)) {
		for (auto& option : options) {

			if (ImGui::Selectable(option.c_str(),
				str == option
			)) {
				self->set_parent_bone(StringName(option.c_str()));
				str = option;
				has_update = true;
			}

		}
		ImGui::EndCombo();
	}

	return has_update;
}


// Inherited via IPropertyEditor

bool ClassTypePtrPropertyEditor::internal_update()
{
	if (!has_init) {
		type_of_base = prop->class_type;
		has_init = true;
	}
	assert(type_of_base);
	
	bool has_update = false;
	const ClassTypeInfo** ptr_prop = (const ClassTypeInfo**)prop->get_ptr(instance);
	const char* preview = (*ptr_prop) ? (*ptr_prop)->classname : "<empty>";
	if (ImGui::BeginCombo("##combocalsstype", preview)) {
		auto subclasses = ClassBase::get_subclasses(type_of_base);
		for (; !subclasses.is_end(); subclasses.next()) {

			if (ImGui::Selectable(subclasses.get_type()->classname,
				subclasses.get_type() == *ptr_prop
			)) {
				*ptr_prop = subclasses.get_type();
				has_update = true;
			}

		}
		ImGui::EndCombo();
	}

	return has_update;
}
