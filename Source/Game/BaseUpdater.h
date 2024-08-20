#pragma once
#include "Framework/ClassBase.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"
CLASS_H(BaseUpdater, ClassBase)
public:
	virtual void update() {}
	bool tickEnabled = false;

	void init_updater();
	void shutdown_updater();
	void set_ticking(bool shouldTick);

	static const PropertyInfoList* get_props() {
		START_PROPS(BaseUpdater)
			REG_BOOL(tickEnabled,PROP_DEFAULT,"0")
		END_PROPS(BaseUpdater)
	}
};