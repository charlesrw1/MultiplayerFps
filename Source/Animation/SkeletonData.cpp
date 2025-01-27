#include "SkeletonData.h"
MSkeleton::~MSkeleton() {
	for (auto clip : clips) {
		if (clip.second.skeleton_owns_clip)
			delete clip.second.ptr;
	}
}
const AnimationSeq* MSkeleton::find_clip(const std::string& name, int& remap_index) const
{
	remap_index = -1;
	auto findthis = clips.find(name);
	if (findthis != clips.end()) {
		remap_index = findthis->second.remap_idx;
		return findthis->second.ptr;
	}
	return nullptr;
}
