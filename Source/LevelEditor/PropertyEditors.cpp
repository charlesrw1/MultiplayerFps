#ifdef EDITOR_BUILD
#define IMGUI_DEFINE_MATH_OPERATORS
#include "PropertyEditors.h"
#include "Framework/FnFactory.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetRegistryLocal.h"
#include "Framework/Config.h"
#include "Assets/AssetBrowser.h"
#include "LevelEditor/EditorDocLocal.h"
#include "Assets/AssetDatabase.h"
#include "Framework/MyImguiLib.h"
#include "Framework/StringUtils.h"
#include "Render/Texture.h"

#include "Game/Components/PhysicsComponents.h"
#include "Game/Components/LightComponents.h"

#include "imgui_internal.h"

int imgui_std_string_resize(ImGuiInputTextCallbackData* data);

bool AssetSlotWidget::draw(const std::string& current_path, const AssetMetadata* metadata,
                           float total_avail, std::string& out_path) {
	ASSERT(metadata);
	auto* drawlist = ImGui::GetWindowDrawList();
	auto& style    = ImGui::GetStyle();
	const float frame_h  = ImGui::GetFrameHeight();
	const float browse_w = frame_h;
	if (total_avail < 0.f)
		total_avail = ImGui::GetContentRegionAvail().x;
	const float slot_w   = total_avail - browse_w - style.ItemSpacing.x;

	bool ret = false;
	std::string new_path;

	// Colored slot background
	ImVec2 slot_min = ImGui::GetCursorScreenPos();
	ImVec2 slot_max = ImVec2(slot_min.x + slot_w, slot_min.y + frame_h);
	{
		Color32 bg = metadata->get_browser_color();
		bg.r = (uint8_t)(bg.r * 0.35f);
		bg.g = (uint8_t)(bg.g * 0.35f);
		bg.b = (uint8_t)(bg.b * 0.35f);
		drawlist->AddRectFilled(slot_min, slot_max, bg.to_uint(), 3.f);
	}
	// Text (clipped to slot)
	ImGui::PushClipRect(slot_min, slot_max, true);
	ImVec2 text_cursor = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(text_cursor.y + style.FramePadding.y * 0.5f);
	if (current_path.empty())
		ImGui::TextDisabled("(none)");
	else
		ImGui::TextUnformatted(current_path.c_str());
	ImGui::PopClipRect();
	// InvisibleButton for interaction
	ImGui::SetCursorPos(text_cursor);
	ImGui::InvisibleButton("##slot", ImVec2(slot_w, frame_h));
	bool slot_hov = ImGui::IsItemHovered();
	// Outline
	ImU32 outline = (slot_hov && current_path.empty())
		? IM_COL32(200, 200, 200, 180) : IM_COL32(180, 180, 180, 50);
	drawlist->AddRect(slot_min, slot_max, outline, 3.f);
	if (slot_hov) {
		ImGui::SetTooltip(current_path.empty() ? "Click to pick or drag an asset here"
		                                       : current_path.c_str());
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
		if (current_path.empty()) {
			ImGui::OpenPopup("##assetpicker");
			picker_filter.clear();
			picker_needs_focus = true;
		} else if (AssetBrowser::inst) {
			AssetBrowser::inst->set_selected(current_path);
			AssetBrowser::inst->force_focus = true;
		}
	}
	// Drag-drop target
	if (ImGui::BeginDragDropTarget()) {
		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
			"AssetBrowserDragDrop", ImGuiDragDropFlags_AcceptPeekOnly);
		if (payload) {
			AssetOnDisk* resource = *(AssetOnDisk**)payload->Data;
			if (resource->type == metadata) {
				if (ImGui::AcceptDragDropPayload("AssetBrowserDragDrop")) {
					new_path = resource->filename;
					ret = true;
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	// Browse icon button (inline, right of slot)
	ImGui::SameLine(0, style.ItemSpacing.x);
	{
		auto browse_tex = g_assets.find<Texture>("eng/icons/doc_search.png");
		const bool has_asset = !current_path.empty();
		ImGui::BeginDisabled(!has_asset);
		ImVec2 btn_pos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##browse", ImVec2(browse_w, frame_h));
		bool browse_hov = ImGui::IsItemHovered();
		ImU32 btn_bg = browse_hov ? IM_COL32(75, 75, 75, 200) : IM_COL32(50, 50, 50, 160);
		drawlist->AddRectFilled(btn_pos, ImVec2(btn_pos.x + browse_w, btn_pos.y + frame_h), btn_bg, 3.f);
		drawlist->AddRect(btn_pos, ImVec2(btn_pos.x + browse_w, btn_pos.y + frame_h), IM_COL32(100, 100, 100, 120), 3.f);
		if (browse_tex) {
			const float ico   = frame_h - style.FramePadding.y * 2.f;
			const float ico_x = btn_pos.x + (browse_w - ico) * 0.5f;
			const float ico_y = btn_pos.y + style.FramePadding.y;
			drawlist->AddImage(
				ImTextureID(uint64_t(browse_tex->get_internal_render_handle())),
				ImVec2(ico_x, ico_y), ImVec2(ico_x + ico, ico_y + ico),
				ImVec2(0, 0), ImVec2(1, 1), has_asset ? IM_COL32(255,255,255,255) : IM_COL32(255,255,255,60));
		}
		ImGui::EndDisabled();
		if (has_asset && browse_hov) ImGui::SetTooltip("Find in browser");
		if (ImGui::IsItemClicked() && has_asset && AssetBrowser::inst) {
			AssetBrowser::inst->set_selected(current_path);
			AssetBrowser::inst->force_focus = true;
		}
	}

	// Picker popup
	ImGui::SetNextWindowSize(ImVec2(320, 360), ImGuiCond_Always);
	if (ImGui::BeginPopup("##assetpicker")) {
		if (picker_needs_focus) { ImGui::SetKeyboardFocusHere(); picker_needs_focus = false; }
		ImGui::SetNextItemWidth(-1.f);
		ImGui::InputText("##picker_filter", (char*)picker_filter.c_str(), picker_filter.size() + 1,
		                 ImGuiInputTextFlags_CallbackResize, imgui_std_string_resize, &picker_filter);
		picker_filter = picker_filter.c_str();
		auto filter_lower = StringUtils::to_lower(picker_filter);
		ImGui::BeginChild("##picker_list", ImVec2(0, 0));
		for (auto* node : AssetRegistrySystem::get().get_linear_list()) {
			if (node->is_folder() || node->asset.type != metadata) continue;
			if (!filter_lower.empty()) {
				auto name_lower = StringUtils::to_lower(node->asset.filename);
				if (name_lower.find(filter_lower) == std::string::npos) continue;
			}
			if (ImGui::Selectable(node->asset.filename.c_str())) {
				new_path = node->asset.filename;
				ret = true;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndChild();
		ImGui::EndPopup();
	}

	if (ret) out_path = new_path;
	return ret;
}

bool SharedAssetPropertyEditor::internal_update() {
	if (!has_init) {
		has_init = true;
		asset_str = get_str();
		const ClassTypeInfo* ct = class_type_override ? class_type_override
		                        : (prop ? prop->class_type : nullptr);
		metadata = ct ? AssetRegistrySystem::get().find_for_classtype(ct) : nullptr;
	}
	if (!metadata) {
		ImGui::Text("Asset has no metadata");
		return false;
	}

	auto* drawlist = ImGui::GetWindowDrawList();
	auto& style = ImGui::GetStyle();
	const float frame_h = ImGui::GetFrameHeight();
	const float v_spacing = style.ItemSpacing.y;
	const float h_spacing = style.ItemSpacing.x;
	const float total_w = ImGui::GetContentRegionAvail().x;
	const float btn_w = frame_h;
	// Thumbnail is square, spans 2 rows + the gap between them
	const float thumb_size = frame_h * 2.f + v_spacing;
	// Right side: [main area | h_spacing | x_btn], then row2: [>> browse full width]
	const float right_w = std::max(total_w - thumb_size - h_spacing, 80.f);
	const float main_w = right_w - h_spacing - btn_w;

	bool ret = false;

	// ---- 1. Thumbnail (spans 2 rows) ----
	ImVec2 thumb_pos = ImGui::GetCursorScreenPos();
	ImVec2 thumb_max = ImVec2(thumb_pos.x + thumb_size, thumb_pos.y + thumb_size);

	// InvisibleButton first so we can query hover/click before drawing
	ImGui::InvisibleButton("##thumb", ImVec2(thumb_size, thumb_size));
	bool thumb_hovered = ImGui::IsItemHovered();
	bool thumb_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

	{
		Texture* thumb = nullptr;
		if (!asset_str.empty() && AssetBrowser::inst) {
			auto* node = AssetBrowser::inst->find_node_for_asset(asset_str);
			if (node && ThumbnailManager::supports_thumbnail(node->asset))
				thumb = AssetBrowser::inst->thumbnails.get_thumbnail(node->asset);
		}
		if (thumb) {
			drawlist->AddImage(ImTextureID(uint64_t(thumb->get_internal_render_handle())), thumb_pos, thumb_max,
							   ImVec2(0, 1), ImVec2(1, 0));
		} else {
			Color32 tc = metadata->get_browser_color();
			drawlist->AddRectFilled(thumb_pos, thumb_max, tc.to_uint(), 4.f);
		}
		// Faint outline; brightens on hover to indicate clickability
		ImU32 outline = thumb_hovered ? IM_COL32(220, 220, 220, 200) : IM_COL32(180, 180, 180, 60);
		drawlist->AddRect(thumb_pos, thumb_max, outline, 4.f, 0, thumb_hovered ? 1.5f : 1.f);
	}

	if (thumb_hovered) {
		ImGui::BeginTooltip();
		ImGui::Text(asset_str.empty() ? "Click to pick a %s" : "Click to find in browser",
					metadata->get_type_name().c_str());
		ImGui::EndTooltip();
	}
	if (thumb_clicked) {
		if (!asset_str.empty()) {
			AssetBrowser::inst->set_selected(asset_str);
			AssetBrowser::inst->force_focus = true;
		} else {
			ImGui::OpenPopup("##assetpicker");
			picker_filter.clear();
			picker_needs_focus = true;
		}
	}

	// ---- 2. Right side: two rows ----
	ImGui::SameLine(0, h_spacing);
	ImGui::BeginGroup();

	// Row 1: [asset name slot][x button]
	{
		ImVec2 slot_min = ImGui::GetCursorScreenPos();
		ImVec2 slot_max = ImVec2(slot_min.x + main_w, slot_min.y + frame_h);

		Color32 bg = metadata->get_browser_color();
		bg.r = (uint8_t)(bg.r * 0.35f);
		bg.g = (uint8_t)(bg.g * 0.35f);
		bg.b = (uint8_t)(bg.b * 0.35f);
		drawlist->AddRectFilled(slot_min, slot_max, bg.to_uint(), 3.f);

		ImGui::PushClipRect(slot_min, slot_max, true);
		ImVec2 text_cursor = ImGui::GetCursorPos();
		ImGui::SetCursorPosY(text_cursor.y + style.FramePadding.y * 0.5f);
		if (get_failed_load())
			ImGui::TextColored(ImColor(Color32{255, 141, 133}.to_uint()), "%s", asset_str.c_str());
		else if (asset_str.empty())
			ImGui::TextDisabled("(none)");
		else
			ImGui::Text("%s", asset_str.c_str());
		ImGui::PopClipRect();

		ImGui::SetCursorPos(text_cursor);
		ImGui::InvisibleButton("##asset_slot", ImVec2(main_w, frame_h));
		bool slot_hov = ImGui::IsItemHovered();

		// Outline: brightens when hovering empty slot to signal it's clickable
		ImU32 outline = (slot_hov && asset_str.empty())
			? IM_COL32(200, 200, 200, 180)
			: IM_COL32(180, 180, 180, 50);
		drawlist->AddRect(slot_min, slot_max, outline, 3.f);

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && asset_str.empty()) {
			ImGui::OpenPopup("##assetpicker");
			picker_filter.clear();
			picker_needs_focus = true;
		}
	}

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
		ImGui::BeginTooltip();
		ImGui::Text("%s", asset_str.empty() ? "Drag a asset here or use the buttons below"
											: asset_str.c_str());
		ImGui::EndTooltip();
	}
	if (ImGui::BeginPopupContextItem("##asset_ctx")) {
		if (ImGui::MenuItem("Copy Path", nullptr, false, !asset_str.empty()))
			ImGui::SetClipboardText(asset_str.c_str());
		const char* clipboard = ImGui::GetClipboardText();
		bool can_paste = clipboard && *clipboard;
		if (ImGui::MenuItem("Paste Path", nullptr, false, can_paste)) {
			set_asset(clipboard);
			asset_str = get_str();
			ret = true;
		}
		if (ImGui::MenuItem("Clear", nullptr, false, !asset_str.empty())) {
			set_asset("");
			asset_str = "";
			ret = true;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Find in Browser", nullptr, false, !asset_str.empty())) {
			AssetBrowser::inst->set_selected(asset_str);
			AssetBrowser::inst->force_focus = true;
		}
		ImGui::EndPopup();
	}
	if (ImGui::BeginDragDropTarget()) {
		const ImGuiPayload* payload =
			ImGui::AcceptDragDropPayload("AssetBrowserDragDrop", ImGuiDragDropFlags_AcceptPeekOnly);
		if (payload) {
			AssetOnDisk* resource = *(AssetOnDisk**)payload->Data;
			if (resource->type == metadata) {
				if ((payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop"))) {
					set_asset(resource->filename);
					asset_str = get_str();
					ret = true;
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	// × clear button (right of name slot)
	ImGui::SameLine(0, h_spacing);
	ImGui::BeginDisabled(asset_str.empty());
	if (ImGui::Button("x##clear", ImVec2(btn_w, frame_h))) {
		set_asset("");
		asset_str = "";
		ret = true;
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && !asset_str.empty()) {
		ImGui::BeginTooltip();
		ImGui::Text("Clear");
		ImGui::EndTooltip();
	}
	ImGui::EndDisabled();

	// Row 2: "Find in browser" icon button — small, left-aligned, disabled when no asset
	{
		auto browse_tex = g_assets.find<Texture>("eng/icons/doc_search.png");
		const bool has_asset = !asset_str.empty();

		ImGui::BeginDisabled(!has_asset);
		ImVec2 btn_pos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##browse", ImVec2(btn_w, frame_h));
		bool browse_hov = ImGui::IsItemHovered();

		ImU32 btn_bg = browse_hov ? IM_COL32(75, 75, 75, 200) : IM_COL32(50, 50, 50, 160);
		drawlist->AddRectFilled(btn_pos, ImVec2(btn_pos.x + btn_w, btn_pos.y + frame_h), btn_bg, 3.f);
		drawlist->AddRect(btn_pos, ImVec2(btn_pos.x + btn_w, btn_pos.y + frame_h), IM_COL32(100, 100, 100, 120), 3.f);

		if (browse_tex) {
			const float ico = frame_h - style.FramePadding.y * 2.f;
			const float ico_x = btn_pos.x + (btn_w - ico) * 0.5f;
			const float ico_y = btn_pos.y + style.FramePadding.y;
			const ImU32 ico_col = has_asset ? IM_COL32(255,255,255,255) : IM_COL32(255,255,255,60);
			drawlist->AddImage(
				ImTextureID(uint64_t(browse_tex->get_internal_render_handle())),
				ImVec2(ico_x, ico_y), ImVec2(ico_x + ico, ico_y + ico),
				ImVec2(0, 0), ImVec2(1, 1), ico_col);
		}
		ImGui::EndDisabled();

		if (has_asset && browse_hov)
			ImGui::SetTooltip("Find in browser");
		if (ImGui::IsItemClicked() && has_asset) {
			AssetBrowser::inst->set_selected(asset_str);
			AssetBrowser::inst->force_focus = true;
		}
	}

	ImGui::EndGroup();

	// ---- Inline asset picker popup ----
	ImGui::SetNextWindowSize(ImVec2(320, 360), ImGuiCond_Always);
	if (ImGui::BeginPopup("##assetpicker")) {
		if (picker_needs_focus) {
			ImGui::SetKeyboardFocusHere();
			picker_needs_focus = false;
		}
		ImGui::SetNextItemWidth(-1.f);
		ImGui::InputText("##picker_filter", (char*)picker_filter.c_str(), picker_filter.size() + 1,
						 ImGuiInputTextFlags_CallbackResize, imgui_std_string_resize, &picker_filter);
		picker_filter = picker_filter.c_str();

		auto filter_lower = StringUtils::to_lower(picker_filter);
		ImGui::BeginChild("##picker_list", ImVec2(0, 0));
		for (auto* node : AssetRegistrySystem::get().get_linear_list()) {
			if (node->is_folder() || node->asset.type != metadata)
				continue;
			if (!filter_lower.empty()) {
				auto name_lower = StringUtils::to_lower(node->asset.filename);
				if (name_lower.find(filter_lower) == std::string::npos)
					continue;
			}
			if (ImGui::Selectable(node->asset.filename.c_str())) {
				set_asset(node->asset.filename);
				asset_str = get_str();
				ret = true;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndChild();
		ImGui::EndPopup();
	}

	return ret;
}

std::string AssetPropertyEditor::get_str() {
	auto ptr = (IAsset**)prop->get_ptr(instance);
	return (*ptr) ? (*ptr)->get_name() : "";
}

void AssetPropertyEditor::set_asset(const std::string& str) {
	auto ptr = (IAsset**)prop->get_ptr(instance);
	if (str.empty()) {
		*ptr = nullptr;
	} else {
		auto classtype = prop->class_type;
		auto asset = g_assets.generic_find(str, classtype).get(); // loader->load_asset(resource->filename);
		*ptr = asset;
	}
}

bool AssetPropertyEditor::get_failed_load() const {
	auto ptr = *(IAsset**)prop->get_ptr(instance);
	if (ptr && ptr->did_load_fail())
		return true;
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

int ColorEditor::extra_row_count() {
	return 0;
}

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

template <typename FUNCTOR>
static bool drag_drop_property_ed_func(std::string* str, Color32 color, FUNCTOR&& callback, const char* targetname,
									   const char* tooltip) {

	// ImGui::PushStyleColor(Imguicolba)

	//	ImGui::InputText("##inp", (char*)str->c_str(), str->size(), ImGuiInputTextFlags_ReadOnly);

	// ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // Ensure no clip rect so mouse hover can reach
	// FramePadding edges
	auto drawlist = ImGui::GetWindowDrawList();
	auto& style = ImGui::GetStyle();
	auto min = ImGui::GetCursorScreenPos();
	auto sz = ImGui::CalcTextSize(str->c_str());
	float width = ImGui::CalcItemWidth();
	drawlist->AddRectFilled(ImVec2(min.x - style.FramePadding.x * 0.5f, min.y),
							ImVec2(min.x + width, min.y + sz.y + style.FramePadding.y * 2.0), color.to_uint());
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
	if (ImGui::BeginDragDropTarget()) {
		// const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		// if (payload->IsDataType("AssetBrowserDragDrop"))
		//	sys_print("``` accepting\n");

		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(targetname)) {
			return_val = callback(payload->Data);
		}
		ImGui::EndDragDropTarget();
	}
	return return_val;
}

#include "Framework/CurveEditorImgui.h"
class GraphCurveEditor : public IPropertyEditor
{
public:
	bool internal_update() final {
		editor.draw();
		return false;
	}
	virtual int extra_row_count() { return 0; }
	virtual bool can_reset() { return false; }
	virtual void reset_value() {}
	CurveEditorImgui editor;
};

void PropertyFactoryUtil::register_basic(FnFactory<IPropertyEditor>& factory) {
	factory.add("BoolButton", []() { return new ButtonPropertyEditor; });
	factory.add("ColorUint", []() { return new ColorEditor; });
	factory.add("AssetPtr", []() { return new AssetPropertyEditor; });
	factory.add("ClassTypePtr", []() { return new ClassTypePtrPropertyEditor; });
	factory.add("GraphCurve", []() { return new GraphCurveEditor; });
}
void PropertyFactoryUtil::register_editor(EditorDoc& doc, FnFactory<IPropertyEditor>& factory) {
	factory.add("EntityBoneParentString", []() { return new EntityBoneParentStringEditor; });
}
void PropertyFactoryUtil::register_anim_editor(AnimationGraphEditor& doc, FnFactory<IPropertyEditor>& factory) {}
void PropertyFactoryUtil::register_mat_editor(MaterialEditorLocal& doc, FnFactory<IPropertyEditor>& factory) {}
#include "Framework/PropertyPtr.h"

void PropertyFactoryUtil::register_anim_editor2(AnimationGraphEditorNew& ed, FnFactory<IPropertyEditor>& factory) {
	// factory.add("StateAliasStruct", [&ed]() {return new StatemachineAliasEditor(ed); });
}

EntityBoneParentStringEditor::~EntityBoneParentStringEditor() {
	StringName* myName = (StringName*)prop->get_ptr(instance);
	*myName = StringName(str.c_str());
}
#include "Game/Components/MeshComponent.h"
#include "Animation/SkeletonData.h"
bool EntityBoneParentStringEditor::internal_update() {
	// cursed!
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
		EntityBoneParentString* val = PropertyPtr(prop, instance).as_struct().get_struct<EntityBoneParentString>();
		assert(val);
		has_init = true;
		StringName* myName = &val->name;
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

		if (set_keyboard_focus) {
			ImGui::SetKeyboardFocusHere();
			set_keyboard_focus = false;
		}
		if (ImGui::InputText("##text", (char*)node_menu_filter_buf.c_str(), node_menu_filter_buf.size() + 1,
							 ImGuiInputTextFlags_CallbackResize, imgui_std_string_resize, &node_menu_filter_buf)) {
			node_menu_filter_buf = node_menu_filter_buf.c_str();
		}

		if (ImGui::Selectable("<empty>", str.empty())) {
			self->set_parent_bone(StringName());
			str.clear();
			has_update = true;
		}
		auto filter_lower = StringUtils::to_lower(node_menu_filter_buf);
		for (auto& option : options) {
			string lower = StringUtils::to_lower(option);
			if (filter_lower.empty() || lower.find(filter_lower) != string::npos) {
				if (ImGui::Selectable(option.c_str(), str == option)) {
					self->set_parent_bone(StringName(option.c_str()));
					str = option;
					has_update = true;
				}
			}
		}
		ImGui::EndCombo();
	} else {
		set_keyboard_focus = true;
	}

	return has_update;
}

// Inherited via IPropertyEditor

bool ClassTypePtrPropertyEditor::internal_update() {
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

			if (ImGui::Selectable(subclasses.get_type()->classname, subclasses.get_type() == *ptr_prop)) {
				*ptr_prop = subclasses.get_type();
				has_update = true;
			}
		}
		ImGui::EndCombo();
	}

	return has_update;
}

#endif