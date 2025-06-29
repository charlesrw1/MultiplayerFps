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
#include "Framework/StringUtils.h"


class SerializeAssetPtr : public IPropertySerializer
{
public:
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		IAsset** ptr_prop = (IAsset**)info.get_ptr(inst);
		if (*ptr_prop) {

			return StringUtils::strip((*ptr_prop)->get_name());	// get the asset path
		}
		else {
			return "";	// return empty
		}
	}

	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user, IAssetLoadingInterface* load) override
	{
		IAsset** ptr_prop = (IAsset**)info.get_ptr(inst);
		
		std::string to_str(token.str_start, token.str_len);

		if (to_str.empty()) {
			*ptr_prop = nullptr;
		}
		else {

			// this does a sync or async depending on the parent request

			auto typeInfo = info.class_type;
			if (typeInfo)
				*ptr_prop = load->load_asset(typeInfo, to_str);// g_assets.find_assetptr_unsafe(to_str, typeInfo);
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
		return StringUtils::strip(ptr->path);
	}
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user, IAssetLoadingInterface* load) override
	{
		SoftAssetPtr<IAsset>* ptr = (SoftAssetPtr<IAsset>*)info.get_ptr(inst);
		ptr->path = std::string(token.str_start, token.str_len);
	}
};
static_assert(sizeof(obj<Entity>) == sizeof(uint64_t), "");

class SerializeEntityPtr : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		assert(0);
		
		return "";
	}

	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user, IAssetLoadingInterface* load) override
	{
		assert(0);
	}
};

ADDTOFACTORYMACRO_NAME(SoftAssetPtrSerializer, IPropertySerializer, "SoftAssetPtr");
ADDTOFACTORYMACRO_NAME(SerializeAssetPtr, IPropertySerializer,	"AssetPtr");
ADDTOFACTORYMACRO_NAME(SerializeEntityPtr, IPropertySerializer, "ObjPtr");

