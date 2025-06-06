#pragma once
#include "Assets/IAsset.h"
#include "Framework/Reflection2.h"
class DataClass : public IAsset {
public:
	CLASS_BODY(DataClass);
	const ClassBase* get_obj() const {
		return object;
	}
	void uninstall() override {
		delete object;
		object = nullptr;
	}
	void sweep_references(IAssetLoadingInterface*) const override;
	void move_construct(IAsset* other) {
		this->object = other->cast_to<DataClass>()->object;
	}
	void post_load() {}
	bool load_asset(IAssetLoadingInterface*);
private:
	ClassBase* object = nullptr;
	friend class DataClassLoadJob;
};
