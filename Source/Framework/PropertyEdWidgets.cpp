#ifdef EDITOR_BUILD
#include "PropertyEd.h"
#include "imgui.h"
#include "Framework/MathLib.h"
#include "FnFactory.h"

// ---- primitive property editor widgets ----

bool StringEditor::internal_update() {
	ASSERT(prop->type == core_type_id::StdString);

	auto str = (std::string*)prop->get_ptr(instance);

	ImguiInputTextCallbackUserStruct user;
	user.string = str;
	if (ImGui::InputText("##input_text", (char*)str->data(), str->size() + 1 /* null terminator byte */,
						 ImGuiInputTextFlags_CallbackResize, imgui_input_text_callback_function, &user)) {
		str->resize(strlen(str->c_str())); // imgui messes with buffer size
		return true;
	}
	return false;
}

bool StringEditor::can_reset() {
	ASSERT(prop->type == core_type_id::StdString);

	auto str = (std::string*)prop->get_ptr(instance);
	return *str != prop->range_hint;
}

void StringEditor::reset_value() {
	ASSERT(prop->type == core_type_id::StdString);

	auto str = (std::string*)prop->get_ptr(instance);
	*str = prop->range_hint;
}

bool FloatEditor::internal_update() {
	ASSERT(prop->type == core_type_id::Float);

	float* ptr = (float*)prop->get_ptr(instance);
	const PropertyAttributes& a = prop->attrs;

	// If min/max are set, use a clamped DragFloat; otherwise InputFloat preserves prior behaviour.
	if (a.has_min() || a.has_max()) {
		float step = a.has_step() ? a.step : 0.05f;
		return ImGui::DragFloat("##input_float", ptr, step, a.min, a.max);
	}
	float step = a.has_step() ? a.step : 0.05f;
	return ImGui::InputFloat("##input_float", ptr, step);
}

bool EnumEditor::internal_update() {
	ASSERT(prop->type == core_type_id::Enum8 || prop->type == core_type_id::Enum16 ||
		   prop->type == core_type_id::Enum32);
	ASSERT(prop->enum_type && prop->enum_type->str_count > 0);
	int64_t myval = prop->get_int(instance);

	auto myenumint = prop->enum_type->find_for_value(myval);
	if (!myenumint) {
		myval = prop->enum_type->strs[0].value;
		prop->set_int(instance, myval);
		myenumint = &prop->enum_type->strs[0];
	}

	bool ret = false;
	if (ImGui::BeginCombo("##type", myenumint->name)) {
		for (auto& enumiterator : *prop->enum_type) {
			bool selected = enumiterator.value == myval;
			if (ImGui::Selectable(enumiterator.name, &selected)) {
				myval = enumiterator.value;
				prop->set_int(instance, myval);
				ret = true;
			}
		}
		ImGui::EndCombo();
	}
	return ret;
}

bool BooleanEditor::internal_update() {
	ASSERT(prop->type == core_type_id::Bool);

	bool b = prop->get_int(instance);
	bool ret = ImGui::Checkbox("##checkbox", &b);

	prop->set_int(instance, b);
	return ret;
}

bool IntegerEditor::internal_update() {
	ASSERT(prop->type == core_type_id::Int8 || prop->type == core_type_id::Int16 || prop->type == core_type_id::Int32 ||
		   prop->type == core_type_id::Int64);

	int val = prop->get_int(instance);
	const PropertyAttributes& a = prop->attrs;

	bool ret = false;
	if (a.has_min() || a.has_max()) {
		float speed = a.has_step() ? a.step : 1.0f;
		int imin = (a.min <= (float)INT_MIN) ? INT_MIN : (int)a.min;
		int imax = (a.max >= (float)INT_MAX) ? INT_MAX : (int)a.max;
		ret = ImGui::DragInt("##input_int", &val, speed, imin, imax);
	} else {
		ret = ImGui::InputInt("##input_int", &val);
	}

	prop->set_int(instance, val);
	return ret;
}

class VectorEditor : public IPropertyEditor
{
public:
	VectorEditor(void* ins, PropertyInfo* inf) {
		ASSERT(ins && inf);
		prop = inf;
		instance = ins;
	}
	virtual bool internal_update() {
		ASSERT(prop && instance);
		glm::vec3* v = (glm::vec3*)prop->get_ptr(instance);
		bool ret = false;
		if (ImGui::DragFloat3("##vec", (float*)v, 0.05))
			ret = true;
		return ret;
	}
};

#include "glm/gtx/euler_angles.hpp"
class RotationEditor : public IPropertyEditor
{
public:
	RotationEditor(void* ins, PropertyInfo* inf) {
		ASSERT(ins && inf);
		prop = inf;
		instance = ins;
		glm::quat* v = (glm::quat*)prop->get_ptr(instance);
		euler = glm::eulerAngles(*v);
		euler *= 180.f / PI;
		lastQuat = *v;
	}

	virtual bool internal_update() {
		ASSERT(prop && instance);
		glm::quat* v = (glm::quat*)prop->get_ptr(instance);

		float dot = glm::dot(lastQuat, *v);
		if (dot < 0.9999) {
			// someone else changed it
			euler = glm::eulerAngles(*v);
			euler *= 180.f / PI;
			lastQuat = *v;
		}

		if (ImGui::DragFloat3("##eul", &euler.x, 1.0)) {
			*v = (glm::quat(euler * PI / 180.f));
			lastQuat = *v;
			return true;
		}
		return false;
	}
	glm::quat lastQuat{};
	glm::vec3 euler{};
};

IPropertyEditor* create_ipropertyed(const FnFactory<IPropertyEditor>& factory, PropertyInfo* prop,
									void* instance, IGridRow* parent) {
	ASSERT(prop && instance);

	IPropertyEditor* out = nullptr;
	out = factory.create(prop->custom_type_str);
	if (out) {
		out->post_construct_for_custom_type(instance, prop, parent);
		return out;
	}

	switch (prop->type) {
	case core_type_id::Bool:
		return new BooleanEditor(instance, prop);
	case core_type_id::Enum8:
	case core_type_id::Enum16:
	case core_type_id::Enum32:
		return new EnumEditor(instance, prop);
	case core_type_id::StdString:
		return new StringEditor(instance, prop);
	case core_type_id::Float:
		return new FloatEditor(instance, prop);
	case core_type_id::Int8:
	case core_type_id::Int16:
	case core_type_id::Int32:
	case core_type_id::Int64:
		return new IntegerEditor(instance, prop);
	case core_type_id::Vec3:
		return new VectorEditor(instance, prop);
	case core_type_id::Quat:
		return new RotationEditor(instance, prop);
	case core_type_id::ObjHandlePtr: {
		out = factory.create("ObjPtr");
	} break;
	case core_type_id::AssetPtr: {
		out = factory.create("AssetPtr");
	} break;
	case core_type_id::ClassTypeInfo: {
		out = factory.create("ClassTypePtr");
	} break;
	}
	if (out) {
		out->post_construct_for_custom_type(instance, prop, parent);
	} else {
		printf("!!!! NO TYPE DEFINED FOR IPropertyEditorFactory %s !!!\n", prop->name);
		return nullptr;
	}
	return out;
}
#endif
