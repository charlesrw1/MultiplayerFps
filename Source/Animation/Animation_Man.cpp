#include "AnimationTreePublic.h"

#include "Framework/DictParser.h"

#include "Render/Model.h"
#include "Framework/Util.h"
#include "SkeletonData.h"
#include <algorithm>
#include "Framework/Files.h"
#include "Assets/AssetDatabase.h"
#include "Animation/Runtime/Animation.h"

CLASS_IMPL(Animation_Tree_CFG);

Animation_Tree_CFG::Animation_Tree_CFG()
{
	
}

Animation_Tree_CFG::~Animation_Tree_CFG()
{
	for (int i = 0; i < all_nodes.size(); i++) {
		delete all_nodes[i];
	}
}

void Animation_Tree_CFG::uninstall()
{
	for (auto node : all_nodes)
		delete node;
	all_nodes.clear();
	root = nullptr;
	direct_slot_names.clear();
}

bool Animation_Tree_CFG::load_asset(ClassBase*& user) {

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


	bool good = read_from_dict(dp);
	if (!good) {
		sys_print(Error, "animation tree file parsing failed \n");
		graph_is_valid = false;
		return false;
	}

	bool valid = post_load_init();

	return true;
}
void Animation_Tree_CFG::move_construct(IAsset* _other) {
	Animation_Tree_CFG* other = (Animation_Tree_CFG*)_other;
	all_nodes = std::move(other->all_nodes);
	root = other->root;
	direct_slot_names = std::move(other->direct_slot_names);
}
AnimatorInstance* Animation_Tree_CFG::allocate_animator_class() const {
	if (!animator_class.ptr)
		return new AnimatorInstance;
	else {
		assert(animator_class.ptr->is_a(AnimatorInstance::StaticType));
		return (AnimatorInstance*)animator_class.ptr->allocate();
	}
}

#include "Framework/PropHashTable.h"

const PropertyInfo* Animation_Tree_CFG::find_animator_instance_variable(const std::string& var_name) const
{
	if (!animator_class.ptr)
		return nullptr;
	StringView sv(var_name.c_str(), var_name.size());
	auto& table = animator_class.ptr->prop_hash_table->prop_table;
	auto find = table.find(sv);
	return find == table.end() ? nullptr : find->second;
}