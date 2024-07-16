#include "SerializePtrHelpers.h"
#include "Framework/AddClassToFactory.h"
#include "Framework/ClassBase.h"
#include "Framework/Util.h"
#include "Assets/AssetLoaderRegistry.h"
#include "Entity.h"

CLASS_IMPL(SerializeEntityObjectContext);

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

			// find factory
			IAssetLoader* loader = AssetLoaderRegistry::get().get_loader_for_type_name(info.range_hint);
			if (!loader) {
				sys_print("!!! no asset loader defined for asset type %s\n", info.range_hint);
				*ptr_prop = nullptr;
			}
			else {
				*ptr_prop = loader->load_asset(to_str);
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

ADDTOFACTORYMACRO_NAME(SerializeObjectPtr, IPropertySerializer, "ObjectPtr");
ADDTOFACTORYMACRO_NAME(SerializeAssetPtr, IPropertySerializer,	"AssetPtr");
ADDTOFACTORYMACRO_NAME(SerializeEntityPtr, IPropertySerializer, "EntityPtr");

