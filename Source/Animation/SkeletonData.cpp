#include "SkeletonData.h"
#include "AnimationUtil.h"

MSkeleton::~MSkeleton() {
	for (auto clip : clips) {
		delete clip.second.ptr;
	}
}
const AnimationSeq* MSkeleton::find_clip(const std::string& name) const
{
	auto findthis = clips.find(name);
	if (findthis != clips.end()) {
		return findthis->second.ptr;
	}
	return nullptr;
}
const BoneIndexRetargetMap* MSkeleton::get_remap(const MSkeleton* other)
{
	if (this == other)
		return nullptr;

	for (auto& remap : remaps) {
		if (remap->who == other)
			return remap.get();
	}
	// have to create remap now, lazily
	auto remap = std::make_unique<BoneIndexRetargetMap>();
	remap->who = other;
	remap->my_skeleton_to_who.resize(bone_dat.size(), -1/* invalid */);
	remap->my_skelton_to_who_quat_delta.resize(bone_dat.size());

	// n^2 :(
	for (int i = 0; i < other->bone_dat.size(); i++) {
		auto& otherb = other->bone_dat[i];
		int j = 0;
		for (; j < bone_dat.size(); j++) {
			auto& myb = bone_dat[j];
			if (myb.name == otherb.name) {
				break;
			}
		}
		if (j != bone_dat.size()) {
			remap->my_skeleton_to_who.at(j) = i;

			//remap->my_skelton_to_who_quat_delta.at(j) = quat_delta(bone_dat.at(j).rot, other->bone_dat.at(i).rot);

		}
	}

	remaps.push_back(std::move(remap));
	return remaps.back().get();
}