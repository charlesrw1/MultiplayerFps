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


static void check_props_for_assetptr(void* inst, const PropertyInfoList* list)
{
	for (int i = 0; i < list->count; i++) {
		auto prop = list->list[i];
		if (strcmp(prop.custom_type_str, "AssetPtr") == 0) {
			// wtf!
			IAsset** e = (IAsset**)prop.get_ptr(inst);
			if (*e)
				AssetDatabase::get().touch_asset(*e);
		}
		else if(prop.type==core_type_id::List) {
			auto listptr = prop.get_ptr(inst);
			auto size = prop.list_ptr->get_size(listptr);
			for (int j = 0; j < size; j++) {
				auto ptr = prop.list_ptr->get_index(listptr, j);
				check_props_for_assetptr(ptr, prop.list_ptr->props_in_list);
			}
		}
	}
}

void Animation_Tree_CFG::sweep_references() const
{
	for (auto obj : all_nodes)
	{
		auto type = &obj->get_type();
		while (type) {
			auto props = type->props;
			if(props)
				check_props_for_assetptr(obj, props);
			type = type->super_typeinfo;
		}
	}
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