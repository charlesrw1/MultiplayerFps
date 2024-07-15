#include "AnimationTreePublic.h"

#include "Framework/DictParser.h"

#include "Model.h"
#include "Framework/Util.h"
#include "SkeletonData.h"
#include <algorithm>
#include "Framework/Files.h"
#include "Assets/AssetLoaderRegistry.h"
static const char* MODEL_DIRECTORY = "Data/Models/";

CLASS_IMPL(Animation_Tree_CFG);
REGISTERASSETLOADER_MACRO(Animation_Tree_CFG, anim_tree_man);

Animation_Tree_CFG* Animation_Tree_Manager::load_animation_tree_file(const char* filename, DictParser& parser)
{
	std::string fullpath = "./Data/Graphs/";
	fullpath += filename;

	auto file = FileSys::open_read_os(fullpath.c_str());

	if (!file) {
		sys_print("!!! couldn't load animation tree file %s\n", filename);
		return nullptr;
	}
	if (file->size() == 0) {
		sys_print("!!! animation tree file empty %s\n", filename);
		return nullptr;
	}
	parser.load_from_file(file.get());


	Animation_Tree_CFG* tree = &trees[filename];
	bool already_loaded = tree->is_initialized();

	if (already_loaded) {
		sys_print("``` tree already loaded, realoading\n");
		// whatev...
		tree->~Animation_Tree_CFG();
		new(tree)Animation_Tree_CFG();
	}

	tree->path = filename;
	bool good = tree->read_from_dict(parser);
	if (!good) {
		sys_print("!!! animation tree file parsing failed %s\n", filename);
		tree->graph_is_valid = false;
		return nullptr;
	}

	bool valid = tree->post_load_init();

	return tree;
}


void Animation_Tree_Manager::init()
{
	
}
