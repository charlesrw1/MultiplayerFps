#pragma once
#include "Framework/MemArena.h"
#include "Framework/Hashset.h"
class MeshComponent;
class AnimatorInstance;
class AnimatorObject;
class SpringBoneManagerComponent;
class GameAnimationMgr
{
public:
	static GameAnimationMgr* inst;

	GameAnimationMgr();
	~GameAnimationMgr();

	void update_animating(); // blocking
	void add_to_animating_set(AnimatorObject& mc);
	void remove_from_animating_set(AnimatorObject& mc);
	// Ticked after every AnimatorObject has updated for the frame, so they can read this
	// frame's animated bone matrices instead of lagging a frame behind.
	void add_to_post_animate_set(SpringBoneManagerComponent& c);
	void remove_from_post_animate_set(SpringBoneManagerComponent& c);
	glm::mat4* get_bonemat_ptr(int ofs) {
		assert(ofs >= 0 && ofs < matricies_allocated);
		return (matricies + ofs);
	}
	int get_num_matricies_used() const { return matricies_used; }

private:
	hash_set<AnimatorObject> animating_meshcomponents;
	hash_set<SpringBoneManagerComponent> post_animate_components;
	glm::mat4* matricies = nullptr;
	int matricies_allocated = 0;
	int matricies_used = 0;
};
