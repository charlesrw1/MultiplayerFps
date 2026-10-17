#ifdef EDITOR_BUILD
#include "PropertyEd.h"
#include "imgui.h"
#include "Framework/MathLib.h"
#include "FnFactory.h"
#include <climits>
#include <cstring>
#include "glm/gtx/euler_angles.hpp"

// ---- primitive property editor widgets ----
// Each edits a domain value (PropValue), so one editor serves every storage of that domain: a float field and a
// LuaSharp double, a std::string, a StringName and a GC string.

namespace {
class PrimitiveEditor : public IPropertyEditor
{
public:
	explicit PrimitiveEditor(const PropertyRef& target) {
		ASSERT(target.info().has_value());
		post_construct(target, nullptr);
	}
};

class StringEditor : public PrimitiveEditor
{
public:
	using PrimitiveEditor::PrimitiveEditor;
	bool internal_update() final {
		std::string text = get<std::string>();
		ImguiInputTextCallbackUserStruct user;
		user.string = &text;
		if (!ImGui::InputText("##input_text", text.data(), text.size() + 1 /* null terminator byte */,
							  ImGuiInputTextFlags_CallbackResize, imgui_input_text_callback_function, &user))
			return false;
		text.resize(strlen(text.c_str())); // imgui messes with buffer size
		set(text);
		return true;
	}
	bool can_reset() final { return get<std::string>() != prop->range_hint; }
	void reset_value() final { set(std::string(prop->range_hint)); }
};

class NumberEditor : public PrimitiveEditor
{
public:
	using PrimitiveEditor::PrimitiveEditor;
	bool internal_update() final {
		double value = get<double>();
		const PropertyAttributes& a = prop->attrs;
		const double step = a.has_step() ? a.step : 0.05;
		bool changed = false;
		// With min/max, a clamped drag; otherwise an input box.
		if (a.has_min() || a.has_max()) {
			const double lo = a.min, hi = a.max;
			changed = ImGui::DragScalar("##input_number", ImGuiDataType_Double, &value, (float)step, &lo, &hi, "%.3f");
		} else {
			changed = ImGui::InputDouble("##input_number", &value, step, 0.0, "%.3f");
		}
		if (changed)
			set(value);
		return changed;
	}
};

class IntegerEditor : public PrimitiveEditor
{
public:
	using PrimitiveEditor::PrimitiveEditor;
	bool internal_update() final {
		int64_t value = get<int64_t>();
		const PropertyAttributes& a = prop->attrs;
		bool changed = false;
		if (a.has_min() || a.has_max()) {
			const float speed = a.has_step() ? a.step : 1.0f;
			const int64_t lo = a.has_min() ? (int64_t)a.min : INT64_MIN;
			const int64_t hi = a.has_max() ? (int64_t)a.max : INT64_MAX;
			changed = ImGui::DragScalar("##input_int", ImGuiDataType_S64, &value, speed, &lo, &hi);
		} else {
			const int64_t one = 1;
			changed = ImGui::InputScalar("##input_int", ImGuiDataType_S64, &value, &one);
		}
		if (changed)
			set(value);
		return changed;
	}
};

class EnumEditor : public PrimitiveEditor
{
public:
	explicit EnumEditor(const PropertyRef& target) : PrimitiveEditor(target) {
		ASSERT(prop->enum_type && prop->enum_type->str_count > 0);
	}
	bool internal_update() final {
		const EnumTypeInfo& type = *prop->enum_type;
		const int64_t value = get<int64_t>();
		const EnumIntPair* current = type.find_for_value(value);
		bool changed = false;
		if (!current) {
			current = &type.strs[0];
			set(current->value);
			changed = true;
		}
		if (ImGui::BeginCombo("##type", current->name)) {
			for (auto& entry : type) {
				if (ImGui::Selectable(entry.name, entry.value == current->value) && entry.value != current->value) {
					set(entry.value);
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}
};

class BooleanEditor : public PrimitiveEditor
{
public:
	using PrimitiveEditor::PrimitiveEditor;
	bool internal_update() final {
		bool b = get<bool>();
		if (!ImGui::Checkbox("##checkbox", &b))
			return false;
		set(b);
		return true;
	}
};

class VectorEditor : public PrimitiveEditor
{
public:
	using PrimitiveEditor::PrimitiveEditor;
	bool internal_update() final {
		glm::vec3 v = get<glm::vec3>();
		if (!ImGui::DragFloat3("##vec", &v.x, 0.05f))
			return false;
		set(v);
		return true;
	}
};

class Vector2Editor : public PrimitiveEditor
{
public:
	using PrimitiveEditor::PrimitiveEditor;
	bool internal_update() final {
		glm::vec2 v = get<glm::vec2>();
		if (!ImGui::DragFloat2("##vec2", &v.x, 0.05f))
			return false;
		set(v);
		return true;
	}
};

class RotationEditor : public PrimitiveEditor
{
public:
	explicit RotationEditor(const PropertyRef& target) : PrimitiveEditor(target) { sync(get<glm::quat>()); }

	bool internal_update() final {
		const glm::quat q = get<glm::quat>();
		if (glm::dot(last_quat, q) < 0.9999f)
			sync(q); // someone else changed it
		if (!ImGui::DragFloat3("##eul", &euler.x, 1.0))
			return false;
		last_quat = glm::quat(euler * PI / 180.f);
		set(last_quat);
		return true;
	}

private:
	void sync(const glm::quat& q) {
		euler = glm::eulerAngles(q) * (180.f / PI);
		last_quat = q;
	}
	glm::quat last_quat{};
	glm::vec3 euler{};
};
} // namespace

std::unique_ptr<IPropertyEditor> create_ipropertyed(const FnFactory<IPropertyEditor>& factory, const PropertyRef& target,
													IGridRow* parent) {
	const PropertyInfo* prop = &target.info();
	std::unique_ptr<IPropertyEditor> out(factory.create(prop->custom_type_str));
	if (out) {
		out->post_construct(target, parent);
		return out;
	}

	switch (prop->type) {
	case core_type_id::Bool:
		return std::make_unique<BooleanEditor>(target);
	case core_type_id::Enum8:
	case core_type_id::Enum16:
	case core_type_id::Enum32:
		return std::make_unique<EnumEditor>(target);
	case core_type_id::StdString:
	case core_type_id::StringName:
	case core_type_id::ScriptString:
		return std::make_unique<StringEditor>(target);
	case core_type_id::Float:
	case core_type_id::Double:
		return std::make_unique<NumberEditor>(target);
	case core_type_id::Int8:
	case core_type_id::Int16:
	case core_type_id::Int32:
	case core_type_id::Int64:
		return std::make_unique<IntegerEditor>(target);
	case core_type_id::Vec2:
		return std::make_unique<Vector2Editor>(target);
	case core_type_id::Vec3:
		return std::make_unique<VectorEditor>(target);
	case core_type_id::Quat:
		return std::make_unique<RotationEditor>(target);
	case core_type_id::ObjHandlePtr:
		out.reset(factory.create("ObjPtr"));
		break;
	case core_type_id::AssetPtr:
		out.reset(factory.create("AssetPtr"));
		break;
	case core_type_id::ClassTypeInfo:
		out.reset(factory.create("ClassTypePtr"));
		break;
	default:
		break;
	}
	if (!out) {
		printf("!!!! NO TYPE DEFINED FOR IPropertyEditorFactory %s !!!\n", prop->name);
		return nullptr;
	}
	out->post_construct(target, parent);
	return out;
}
#endif
