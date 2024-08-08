#pragma once

#include <memory>
#include "Framework/Handle.h"
#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"



class Model;
class Animator_Tree_CFG;
class PhysicsActor;
class Animation_Tree_CFG;
class AnimatorInstance;
class Render_Object;
class MaterialInstance;
class Texture;


#if 0
CLASS_H(TimeSeqComponent, EntityComponent)
public:
	void play();
	void stop();
	bool is_playing() const;
	float get_percentage() const;

	void on_init() override;

	// ticks the timeline if playing
	void on_tick() override;

	// gets a curve value
	float get_curve_value(StringName curve_name);

	// MulticastDelegate OnFinished;	called when timeline reaches the end
	// MulticastDelegate OnTick;		called every tick its playing
};
#endif
