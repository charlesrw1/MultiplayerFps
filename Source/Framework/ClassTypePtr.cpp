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
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user,IAssetLoadingInterface*) override
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


#endif