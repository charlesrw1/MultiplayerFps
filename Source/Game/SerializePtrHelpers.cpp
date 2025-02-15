#include "SerializePtrHelpers.h"
#include "Framework/AddClassToFactory.h"
#include "Framework/ClassBase.h"
#include "Framework/Util.h"
#include "Game/Entity.h"
#include "Assets/AssetDatabase.h"
#include "Level.h"
#include "GameEnginePublic.h"

#include "LevelSerialization/SerializationAPI.h"
#include "Framework/ReflectionProp.h"
#include "Framework/AddClassToFactory.h"
#include "Framework/StringUtil.h"
#include "SoftAssetPtr.h"

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
				*ptr_prop = g_assets.find_assetptr_unsafe(to_str, typeInfo);
			else {
				sys_print(Error, "no asset loader defined for asset type %s\n", info.range_hint);
				*ptr_prop = nullptr;
			}

		}
	}
};
class SoftAssetPtrSerializer : public IPropertySerializer
{
public:
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		SoftAssetPtr<IAsset>* ptr = (SoftAssetPtr<IAsset>*)info.get_ptr(inst);
		return ptr->path;
	}
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user) override
	{
		SoftAssetPtr<IAsset>* ptr = (SoftAssetPtr<IAsset>*)info.get_ptr(inst);
		ptr->path = std::string(token.str_start, token.str_len);
	}
};

class SerializeEntityPtr : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		ASSERT(user->is_a<LevelSerializationContext>());
		auto ctx = (LevelSerializationContext*)user;
		uint64_t handle = *(uint64_t*)info.get_ptr(inst);
		if (handle == 0)
			return "";
		auto oent = ctx->get_entity(handle);
		if (!oent) {
			sys_print(Warning, "handle wasnt found when serializing: %d", handle);
			return "";
		}

		auto ent = oent->cast_to<Entity>();

		auto from = build_path_for_object((BaseUpdater*)ctx->cur_obj,nullptr);
		auto to = build_path_for_object((BaseUpdater*)ent,nullptr);

		return serialize_build_relative_path(from.c_str(),to.c_str());
	}

	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user) override
	{
		ASSERT(user->is_a<LevelSerializationContext>());
		auto ctx = (LevelSerializationContext*)user;
		uint64_t* p = (uint64_t*)info.get_ptr(inst);

		auto stack = token.to_stack_string();
		if (stack.size() == 0) {
			*p = 0;
			return;
		}

		auto path = unserialize_relative_to_absolute(stack.c_str(), ctx->in_root->c_str());
		auto find = ctx->in->find(path);
		if (!find) {
			sys_print(Error, "couldnt find path for entityptr %s\n", path.c_str());
		}
		else {
			ASSERT(find->is_a<Entity>());
			*p = reinterpret_cast<uint64_t>(find);	// to get fixed up later

			ctx->in->add_entityptr_refer((BaseUpdater*)ctx->cur_obj);
		}
	}
};

ADDTOFACTORYMACRO_NAME(SoftAssetPtrSerializer, IPropertySerializer, "SoftAssetPtr");
ADDTOFACTORYMACRO_NAME(SerializeAssetPtr, IPropertySerializer,	"AssetPtr");
ADDTOFACTORYMACRO_NAME(SerializeEntityPtr, IPropertySerializer, "EntityPtr");

