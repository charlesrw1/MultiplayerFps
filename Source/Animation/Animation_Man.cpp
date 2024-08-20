#include "AnimationTreePublic.h"

#include "Framework/DictParser.h"

#include "Render/Model.h"
#include "Framework/Util.h"
#include "SkeletonData.h"
#include <algorithm>
#include "Framework/Files.h"
#include "Assets/AssetDatabase.h"

#include "Framework/ExpressionLang.h"

static const char* MODEL_DIRECTORY = "Data/Models/";

CLASS_IMPL(Animation_Tree_CFG);

Animation_Tree_CFG::Animation_Tree_CFG()
{
	code = std::make_unique<Script>();
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
	code.reset();
	data_used = 0;
	direct_slot_names.clear();
}

bool Animation_Tree_CFG::load_asset(ClassBase*& user) {

	auto& path = get_name();
	std::string fullpath = "./Data/Graphs/";
	fullpath += get_name();

	auto file = FileSys::open_read_os(fullpath.c_str());

	if (!file) {
		sys_print("!!! couldn't load animation tree file %s\n", path.c_str());
		return false;
	}
	if (file->size() == 0) {
		sys_print("!!! animation tree file empty %s\n", path.c_str());
		return false;
	}

	DictParser dp;
	dp.load_from_file(file.get());


	bool good = read_from_dict(dp);
	if (!good) {
		sys_print("!!! animation tree file parsing failed \n");
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
	code = std::move(other->code);
	data_used = other->data_used;
	direct_slot_names = std::move(other->direct_slot_names);
}
