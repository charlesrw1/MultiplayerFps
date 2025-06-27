#pragma once
#include "Framework/MemArena.h"
#include "Framework/Hashset.h"
class MeshComponent;
class AnimatorInstance;
class AnimatorObject;
class GameAnimationMgr
{
public:
	GameAnimationMgr();
	~GameAnimationMgr();
	void init();
	void update_animating();	// blocking
	void add_to_animating_set(AnimatorObject& mc);
	void remove_from_animating_set(AnimatorObject& mc);
	glm::mat4* get_bonemat_ptr(int ofs) {
		assert(ofs >= 0 && ofs < matricies_allocated);
		return (matricies + ofs);
	}
	int get_num_matricies_used() const {
		return matricies_used;
	}
private:
	hash_set<AnimatorObject> animating_meshcomponents;
	glm::mat4* matricies = nullptr;
	int matricies_allocated = 0;
	int matricies_used = 0;
};
extern GameAnimationMgr g_gameAnimationMgr;