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
	PrimitiveEditor(void* inst, PropertyInfo* p) {
		ASSERT(inst && p && p->has_value());
		instance = inst;
		prop = p;
	}
};

class StringEditor : public PrimitiveEditor
{
public:
	using PrimitiveEditor::PrimitiveEditor;
	bool internal_update() final {
		std::string text = prop->get<std::string>(instance);
		ImguiInputTextCallbackUserStruct user;
		user.string = &text;
		if (!ImGui::InputText("##input_text", text.data(), text.size() + 1 /* null terminator byte */,
							  ImGuiInputTextFlags_CallbackResize, imgui_input_text_callback_function, &user))
			return false;
		text.resize(strlen(text.c_str())); // imgui messes with buffer size
		prop->set(instance, text);
		return true;
	}
	bool can_reset() final { return prop->get<std::string>(instance) != prop->range_hint; }
	void reset_value() final { prop->set(instance, std::string(prop->range_hint)); }
};

class NumberEditor : public PrimitiveEditor
{
public:
	using PrimitiveEditor::PrimitiveEditor;
	bool internal_update() final {
		double value = prop->get<double>(instance);
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
			prop->set(instance, value);
		return changed;
	}
};

class IntegerEditor : public PrimitiveEditor
{
public:
	using PrimitiveEditor::PrimitiveEditor;
	bool internal_update() final {
		int64_t value = prop->get<int64_t>(instance);
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
			prop->set(instance, value);
		return changed;
	}
};

class EnumEditor : public PrimitiveEditor
{
public:
	EnumEditor(void* inst, PropertyInfo* p) : PrimitiveEditor(inst, p) {
		ASSERT(prop->enum_type && prop->enum_type->str_count > 0);
	}
	bool internal_update() final {
		const EnumTypeInfo& type = *prop->enum_type;
		const int64_t value = prop->get<int64_t>(instance);
		const EnumIntPair* current = type.find_for_value(value);
		bool changed = false;
		if (!current) {
			current = &type.strs[0];
			prop->set(instance, current->value);
			changed = true;
		}
		if (ImGui::BeginCombo("##type", current->name)) {
			for (auto& entry : type) {
				if (ImGui::Selectable(entry.name, entry.value == current->value) && entry.value != current->value) {
					prop->set(instance, entry.value);
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
		bool b = prop->get<bool>(instance);
		if (!ImGui::Checkbox("##checkbox", &b))
			return false;
		prop->set(instance, b);
		return true;
	}
};

class VectorEditor : public PrimitiveEditor
{
public:
	using PrimitiveEditor::PrimitiveEditor;
	bool internal_update() final {
		glm::vec3 v = prop->get<glm::vec3>(instance);
		if (!ImGui::DragFloat3("##vec", &v.x, 0.05f))
			return false;
		prop->set(instance, v);
		return true;
	}
};

class Vector2Editor : public PrimitiveEditor
{
public:
	using PrimitiveEditor::PrimitiveEditor;
	bool internal_update() final {
		glm::vec2 v = prop->get<glm::vec2>(instance);
		if (!ImGui::DragFloat2("##vec2", &v.x, 0.05f))
			return false;
		prop->set(instance, v);
		return true;
	}
};

class RotationEditor : public PrimitiveEditor
{
public:
	RotationEditor(void* inst, PropertyInfo* p) : PrimitiveEditor(inst, p) { sync(prop->get<glm::quat>(instance)); }

	bool internal_update() final {
		const glm::quat q = prop->get<glm::quat>(instance);
		if (glm::dot(last_quat, q) < 0.9999f)
			sync(q); // someone else changed it
		if (!ImGui::DragFloat3("##eul", &euler.x, 1.0))
			return false;
		last_quat = glm::quat(euler * PI / 180.f);
		prop->set(instance, last_quat);
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

std::unique_ptr<IPropertyEditor> create_ipropertyed(const FnFactory<IPropertyEditor>& factory, PropertyInfo* prop,
													void* instance, IGridRow* parent) {
	ASSERT(prop && instance);

	std::unique_ptr<IPropertyEditor> out(factory.create(prop->custom_type_str));
	if (out) {
		out->post_construct_for_custom_type(instance, prop, parent);
		return out;
	}

	switch (prop->type) {
	case core_type_id::Bool:
		return std::make_unique<BooleanEditor>(instance, prop);
	case core_type_id::Enum8:
	case core_type_id::Enum16:
	case core_type_id::Enum32:
		return std::make_unique<EnumEditor>(instance, prop);
	case core_type_id::StdString:
	case core_type_id::StringName:
	case core_type_id::ScriptString:
		return std::make_unique<StringEditor>(instance, prop);
	case core_type_id::Float:
	case core_type_id::Double:
		return std::make_unique<NumberEditor>(instance, prop);
	case core_type_id::Int8:
	case core_type_id::Int16:
	case core_type_id::Int32:
	case core_type_id::Int64:
		return std::make_unique<IntegerEditor>(instance, prop);
	case core_type_id::Vec2:
		return std::make_unique<Vector2Editor>(instance, prop);
	case core_type_id::Vec3:
		return std::make_unique<VectorEditor>(instance, prop);
	case core_type_id::Quat:
		return std::make_unique<RotationEditor>(instance, prop);
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
	out->post_construct_for_custom_type(instance, prop, parent);
	return out;
}
#endif
