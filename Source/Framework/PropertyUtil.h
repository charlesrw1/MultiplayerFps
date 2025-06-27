#pragma once
#include "ReflectionProp.h"
#include "Assets/IAsset.h"
#include "Assets/AssetDatabase.h"
inline void check_props_for_assetptr(void* inst, const PropertyInfoList* list, IAssetLoadingInterface* load)
{
	for (int i = 0; i < list->count; i++) {
		auto& prop = list->list[i];
		if (prop.type == core_type_id::AssetPtr) {
			// wtf!
			IAsset** e = (IAsset**)prop.get_ptr(inst);
			if (*e)
				load->touch_asset(*e);
		}
		else if (prop.type == core_type_id::List) {
			auto listptr = prop.get_ptr(inst);
			auto size = prop.list_ptr->get_size(listptr);
			for (int j = 0; j < size; j++) {
				auto ptr = prop.list_ptr->get_index(listptr, j);
				check_props_for_assetptr(ptr, prop.list_ptr->props_in_list, load);
			}
		}
	}
}
inline void check_object_for_asset_ptr(ClassBase* obj, IAssetLoadingInterface* load)
{
	auto type = &obj->get_type();
	while (type) {
		auto props = type->props;
		if (props)
			check_props_for_assetptr(obj, props, load);
		type = type->super_typeinfo;
	}
}