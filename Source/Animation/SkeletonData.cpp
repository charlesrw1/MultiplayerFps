#include "SkeletonData.h"
#include "AnimationUtil.h"

MSkeleton::~MSkeleton() {
	for (auto& clip : clips) {
		delete clip.second.ptr;
	}
}
void MSkeleton::move_construct(MSkeleton& other)
{
	//fixme: edge case where you reload a model with more/less bones but have leftover stale animations from prev
	remaps = std::move(other.remaps);
	mirroring_table = std::move(other.mirroring_table);
	bone_dat = std::move(other.bone_dat);
	for (auto& [anim_name, clip] : other.clips) {
		assert(clip.ptr);
		auto findMe = clips.find(anim_name);
		if (findMe != clips.end()) {
			assert(findMe->second.ptr);
			*findMe->second.ptr = std::move(*clip.ptr);	// move construct it
		}
		else {
			clips.insert({ anim_name,clip });
			clip.ptr = nullptr;	// steal it
		}
	}
}
bool MSkeleton::is_skeleton_the_same(const MSkeleton& other) const {
	if (get_num_bones() != other.get_num_bones())
		return false;
	for (int i = 0; i < get_num_bones(); i++) {
		if (bone_dat[i].name != other.bone_dat[i].name)
			return false;
	}
	return true;
}
int MSkeleton::get_bone_index(StringName name) const {
	for (int i = 0; i < bone_dat.size(); i++) {
		if (bone_dat[i].name == name)
			return i;
	}
	return -1;
}
const AnimationSeq* MSkeleton::find_clip(const std::string& name) const
{
	auto findthis = clips.find(name);
	if (findthis != clips.end()) {
		return findthis->second.ptr;
	}
	return nullptr;
}
AnimationSeq* MSkeleton::find_clip(const std::string& name)
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

