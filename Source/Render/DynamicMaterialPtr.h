#pragma once
#include <memory>
class MaterialInstance;
struct DynamicMaterialDeleter
{
	void operator()(MaterialInstance* m) const;
};
using DynamicMatUniquePtr = std::unique_ptr<MaterialInstance, DynamicMaterialDeleter>;