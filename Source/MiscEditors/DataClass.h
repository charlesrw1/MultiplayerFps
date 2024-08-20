#pragma once

// A data class is simply a ClassBase object that can be serialized and edited
// Allows for data driven assets in a really easy way!!!
// Just create a ClassBase inherited object, then open the editor with the classes name ("start_ed DataClass <ClassName>")
// This has the advantage of not needing manual parsers and it integrates 
// with all custom property editors! (like assets or other ClassBase's, just drag and drop, omg I love this so much)

#include "Assets/IAsset.h"

CLASS_H(DataClass, IAsset)
public:
	const ClassBase* get_obj() const {
		return object;
	}
	void uninstall() override {
		delete object;
		object = nullptr;
	}
	void sweep_references() const override;
	void move_construct(IAsset* other) {
		this->object = other->cast_to<DataClass>()->object;
	}
	void post_load(ClassBase*) {}
	bool load_asset(ClassBase*&);
private:
	ClassBase* object = nullptr;

	friend class DataClassLoadJob;
};
