#include "ClassTypePtr.h"
#include "Framework/AddClassToFactory.h"
class ClassTypePtrSerializer : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		const ClassTypeInfo** ptr_prop = (const ClassTypeInfo**)info.get_ptr(inst);
		if (!(*ptr_prop)) {
			return "";	// empty
		}
		else {
			return (*ptr_prop)->classname;
			// FIXME: add type checking here too!!
		}
	}
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user) override
	{
		const ClassTypeInfo** ptr_prop = (const ClassTypeInfo**)info.get_ptr(inst);

		std::string to_str(token.str_start, token.str_len);

		if (to_str.empty()) {
			*ptr_prop = nullptr;
		}
		else {
			*ptr_prop = ClassBase::find_class(to_str.c_str());
			// FIXME: type checking!!

		}
	}
};
ADDTOFACTORYMACRO_NAME(ClassTypePtrSerializer, IPropertySerializer, "ClassTypePtr");

#ifdef EDITOR_BUILD
class ClassTypePtrPropertyEditor : public IPropertyEditor
{
public:
	// Inherited via IPropertyEditor
	virtual bool internal_update() override
	{
		if (!has_init) {
			type_of_base = ClassBase::find_class(prop->range_hint);
			has_init = true;
		}
		if (!type_of_base) {
			ImGui::Text("Couldnt find base class: %s\n", prop->range_hint);
			return false;
		}

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
	bool has_init = false;

	const ClassTypeInfo* type_of_base = nullptr;
};

ADDTOFACTORYMACRO_NAME(ClassTypePtrPropertyEditor, IPropertyEditor, "ClassTypePtr");
#endif