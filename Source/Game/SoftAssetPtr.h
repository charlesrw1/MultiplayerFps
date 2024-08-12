#pragma once

#include "Framework/ReflectionProp.h"
#include "Framework/ArrayReflection.h"

// like an AssetPtr but doesnt load the asset when its referenced, just has a path
// map/level asset types can only be referenced this way (AssetPtr's wont work)
// other asset type that works with AssetPtr can be referenced this way if you want to avoid loading unneeded data
// but have the ability to set the path in the editor
template<typename T>
class SoftAssetPtr
{
public:
	~SoftAssetPtr() {
		static_assert(std::is_base_of<IAsset, T>::value, "SoftAssetPtr must derive from IAsset");
	}
	const T* load() const;
	std::string path;
};

template<typename T>
inline PropertyInfo make_soft_asset_ptr_property(const char* name, uint16_t offset, uint32_t flags, SoftAssetPtr<T>* dummy) {
	return make_struct_property(name, offset, flags, "SOFT_AssetPtr", T::StaticType.classname);
}

#define REG_SOFT_ASSET_PTR(name,flags)  make_soft_asset_ptr_property(#name, offsetof(MyClassType,name),flags,&((MyClassType*)0)->name)

template<typename T>
struct GetAtomValueWrapper<SoftAssetPtr<T>> {
	static PropertyInfo get() {
		return make_struct_property(name, offset, flags, "SOFT_AssetPtr", T::StaticType.classname);
	}
};
