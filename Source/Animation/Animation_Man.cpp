#include "AnimationTreePublic.h"

#include "Framework/DictParser.h"

#include "Render/Model.h"
#include "Framework/Util.h"
#include "SkeletonData.h"
#include <algorithm>
#include "Framework/Files.h"
#include "Assets/AssetDatabase.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/Runtime/AnimationTreeLocal.h"


Animation_Tree_CFG::Animation_Tree_CFG()
{
	animator_class = AnimatorInstance::StaticType;
}

Animation_Tree_CFG::~Animation_Tree_CFG()
{
	
}


void Animation_Tree_CFG::uninstall()
{
	
	direct_slot_names.clear();
}

#include "Framework/PropertyUtil.h"

void Animation_Tree_CFG::sweep_references(IAssetLoadingInterface* load) const
{
	
}
bool Animation_Tree_CFG::load_asset(IAssetLoadingInterface* load) {

	auto& path = get_name();

	auto file = FileSys::open_read_game(path);

	if (!file) {
		sys_print(Error, "couldn't load animation tree file %s\n", path.c_str());
		return false;
	}
	if (file->size() == 0) {
		sys_print(Error, "animation tree file empty %s\n", path.c_str());
		return false;
	}

	DictParser dp;
	dp.load_from_file(file.get());

	bool valid = post_load_init();

	return true;
}
void Animation_Tree_CFG::move_construct(IAsset* _other) {
	
}
AnimatorInstance* Animation_Tree_CFG::allocate_animator_class() const {
	if (!animator_class.ptr)
		return new AnimatorInstance;
	else {
		assert(animator_class.ptr->is_a(AnimatorInstance::StaticType));
		return (AnimatorInstance*)animator_class.ptr->allocate();
	}
}
// Inherited via At_Node



bool Animation_Tree_CFG::post_load_init()
{
	return true;
}

