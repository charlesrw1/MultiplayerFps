#include "SerializePtrHelpers.h"
#include "Framework/AddClassToFactory.h"
#include "Framework/ClassBase.h"
#include "Framework/Util.h"
#include "Game/Entity.h"
#include "Assets/AssetDatabase.h"

CLASS_IMPL(SerializeEntityObjectContext);

#if 0
class SerializeObjectPtr : public IPropertySerializer
{
public:
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		assert(user);
		auto ctx = user->cast_to<SerializeEntityObjectContext>();
		assert(ctx);

		void** ptr_prop = (void**)info.get_ptr(inst);

		auto find = ctx->to_serialize_index.find(*ptr_prop);
		if (find == ctx->to_serialize_index.end()) {
			sys_print("!!! Couldn't find ObjectPtr to serialize %s\n", info.name);
			return std::string("0");
		}
		else {
			return std::to_string(find->second);
		}
	}

	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user) override
	{
		// unserializing has no context, gets fixup at a later step
		void** ptr_prop = (void**)info.get_ptr(inst);

		auto stack = token.to_stack_string();
		uint32_t res = 0;
		int number = sscanf(stack.c_str(), "%d", &res);
		if (number != 1) {
			sys_print("!!! Error on ObjectPtr unserialize\n");
			res = 0;
		}
		uintptr_t* ptr_prop_as_int = (uintptr_t*)(*ptr_prop);
		*ptr_prop_as_int = res;
	}
};
#endif


class SerializeAssetPtr : public IPropertySerializer
{
public:
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		IAsset** ptr_prop = (IAsset**)info.get_ptr(inst);
		if (*ptr_prop) {

			return (*ptr_prop)->get_name();	// get the asset path
		}
		else {
			return "";	// return empty
		}
	}

	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user) override
	{
		IAsset** ptr_prop = (IAsset**)info.get_ptr(inst);
		
		std::string to_str(token.str_start, token.str_len);

		if (to_str.empty()) {
			*ptr_prop = nullptr;
		}
		else {

			// this does a sync or async depending on the parent request

			auto typeInfo = ClassBase::find_class(info.range_hint);
			if (typeInfo)
				*ptr_prop = GetAssets().find_assetptr_unsafe(to_str, typeInfo);
			else {
				sys_print("!!! no asset loader defined for asset type %s\n", info.range_hint);
				*ptr_prop = nullptr;
			}

		}
	}
};

class SerializeEntityPtr : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		uint64_t* p = (uint64_t*)info.get_ptr(inst);
		return std::to_string(*p);
	}

	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user) override
	{
		uint64_t* p = (uint64_t*)info.get_ptr(inst);
		auto stack = token.to_stack_string();
		int fields = sscanf(stack.c_str(), "%llu", p);
		if (fields != 1) {
			sys_print("!!! unserialize EntityPtr error\n");
			*p = 0;
		}
	}
};

class SerializeECPtr : public IPropertySerializer
{
public:
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		assert(user);
		auto ctx = user->cast_to<SerializeEntityObjectContext>();
		assert(ctx);

		ObjPtr<EntityComponent>* ptr_prop = (ObjPtr<EntityComponent>*)info.get_ptr(inst);

		if (!ptr_prop->get())
			return "";
		else
			return ptr_prop->get()->eSelfNameString;
	}

	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user) override
	{
		// unserializing has no context, gets fixup at a later step

		assert(user);
		auto ctx = user->cast_to<SerializeEntityObjectContext>();
		assert(ctx);
		assert(ctx->entity_serialzing);

		auto stack = token.to_stack_string();
		ObjPtr<EntityComponent>* ptr_prop = (ObjPtr<EntityComponent>*)info.get_ptr(inst);

		if (stack.size() == 0) {
			ptr_prop->ptr = nullptr;
		}
		else {
			ptr_prop->ptr = ctx->entity_serialzing->find_component_for_string_name(stack.c_str());
		}
	}
};
ADDTOFACTORYMACRO_NAME(SerializeECPtr, IPropertySerializer, "EntityCompPtr");
ADDTOFACTORYMACRO_NAME(SerializeAssetPtr, IPropertySerializer,	"AssetPtr");
ADDTOFACTORYMACRO_NAME(SerializeEntityPtr, IPropertySerializer, "EntityPtr");

