#include "AnimationTreePublic.h"

#include "Framework/DictParser.h"

#include "Model.h"
#include "Framework/Util.h"
#include "SkeletonData.h"
#include <algorithm>

static const char* GRAPH_DIRECTORY = "Data/Animations/Graphs/";
static const char* SKEL_DIRECTORY = "Data/Animations/Skels/";
static const char* MODEL_DIRECTORY = "Data/Models/";
static const char* NOTIFY_DIRECTORY = "Data/Animations/Notify/";
static const char* SET_DIRECTORY = "Data/Animations/Sets/";


Animation_Tree_CFG* Animation_Tree_Manager::load_animation_tree_file(const char* filename, DictParser& parser)
{
	if (!parser.load_from_file(filename)) {
		printf("couldn't load animation tree file %s\n", filename);
		return nullptr;
	}
	Animation_Tree_CFG* tree = &trees[filename];
	bool already_loaded = tree->is_initialized();

	if (already_loaded) {
		printf("tree already loaded, realoading\n");
		// whatev...
		tree->~Animation_Tree_CFG();
		new(tree)Animation_Tree_CFG();
	}

	tree->name = filename;
	bool good = tree->read_from_dict(parser);
	if (!good) {
		printf("animation tree file parsing failed %s\n", filename);
		tree->graph_is_valid = false;
		return nullptr;
	}

	tree->post_load_init();

	return tree;
}


void Animation_Tree_Manager::init()
{
	
}
