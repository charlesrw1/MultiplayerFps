#pragma once

#include "Framework/ClassBase.h"


enum class PhysicsChannel : uint8_t
{
	StaticObject,	// static world objects (like static meshes)
	DynamicObject,	// world objects being moved by code
	PhysicsObject,	// world objects beign simulated by the physics engine
	Character,		// any player or ai objects
	Visiblity,		// any ray casts of the world
};

// Abusing class reflection like an enum, but I really like the property of it being staticlly compiled but also extenable (and it easily plugs in already to the reflection system for properties).
// physics presets, these are an instance of a channel along with masks of other physics channels
CLASS_H(PhysicsFilterPresetBase, ClassBase)
public:
	const static bool CreateDefaultObject = true;	// create a default object

	// "default" is to just ignore everything
	uint32_t blockMask = 0;
	uint32_t overlapMask = 0;
	PhysicsChannel physicsChannelBaseType = PhysicsChannel::StaticObject;
	enum MaskType {
		Ignore,
		Overlap,
		Block
	};
	bool isQueryable = true;
	bool hasCollisions = true;
protected:
	void set_self(PhysicsChannel self) {
		physicsChannelBaseType = self;
	}
	void set_default(MaskType mt) {
		blockMask = overlapMask = 0;
		if (mt == Overlap)
			overlapMask = UINT32_MAX;
		else if (mt == Block)
			blockMask = overlapMask = UINT32_MAX;
	}
	void set_state(PhysicsChannel channel, MaskType mt) {
		uint32_t as_int = (uint32_t)channel;
		if (mt == Ignore) {
			blockMask = blockMask & ~(1 << as_int);
			overlapMask = overlapMask & ~(1 << as_int);
		}
		else if (mt == Overlap) {
			overlapMask |= (1 << as_int);
			blockMask = blockMask & ~(1 << as_int);
		}
		else if (mt == Block) {
			overlapMask |= (1 << as_int);
			blockMask |= (1 << as_int);
		}
	}
};
CLASS_H(PP_DefaultBlockAll, PhysicsFilterPresetBase)
public:
	PP_DefaultBlockAll();
};
CLASS_H(PP_Character, PhysicsFilterPresetBase)
public:
	PP_Character();
};
CLASS_H(PP_Trigger, PhysicsFilterPresetBase)
public:
	PP_Trigger();
};
CLASS_H(PP_PhysicsEntity, PhysicsFilterPresetBase)
public:
	PP_PhysicsEntity();
};
CLASS_H(PP_NoCollision, PhysicsFilterPresetBase)
public:
	PP_NoCollision();
};

