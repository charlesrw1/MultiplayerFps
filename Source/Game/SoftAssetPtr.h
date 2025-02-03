#pragma once

#include "Framework/ReflectionProp.h"
#include "Framework/ArrayReflection.h"
#include "Assets/IAsset.h"

// like an AssetPtr but doesnt load the asset when its referenced, just has a path
// map/level asset types can only be referenced this way (AssetPtr's wont work)
template<typename T>
class SoftAssetPtr
{
public:
	SoftAssetPtr() {}
	SoftAssetPtr(const std::string& path) : path(path) {}
	~SoftAssetPtr() {
		static_assert(std::is_base_of<IAsset, T>::value, "SoftAssetPtr must derive from IAsset");
	}
	const T* load() const;
	std::string path;
};

template<typename T>
inline PropertyInfo make_soft_asset_ptr_property(const char* name, uint16_t offset, uint32_t flags, SoftAssetPtr<T>* dummy) {
	return make_struct_property(name, offset, flags, "SoftAssetPtr", T::StaticType.classname);
}

#define REG_SOFT_ASSET_PTR(name,flags)  make_soft_asset_ptr_property(#name, offsetof(MyClassType,name),flags,&((MyClassType*)0)->name)

template<typename T>
struct GetAtomValueWrapper<SoftAssetPtr<T>> {
	static PropertyInfo get() {
		return make_struct_property("", 0, PROP_DEFAULT, "SoftAssetPtr", T::StaticType.classname);
	}
};
