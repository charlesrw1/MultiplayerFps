#include "AnimationTreePublic.h"

#include "DictParser.h"

#include "Model.h"
#include "Util.h"

static const char* GRAPH_DIRECTORY = "Data/Animations/Graphs/";
static const char* SKEL_DIRECTORY = "Data/Animations/Skels/";
static const char* MODEL_DIRECTORY = "Data/Models/";
static const char* SET_DIRECTORY = "Data/Animations/Sets/";


// expected that '{' was read
static bool read_import(DictParser& parser, StringView& str,Animation_Set_New* set)
{

	const Model* model_temp = nullptr;


	Model_Skeleton* skel = nullptr;

	parser.read_string(str);
	while (!parser.check_item_end(str) && !parser.is_eof()) {
		if (str.cmp("source")) {
			parser.read_string(str);
			model_temp = mods.find_or_load(str.to_stack_string().c_str());
			if (!model_temp || !model_temp->animations) {
				printf("Couldn't find import model\n");
				return false;
			}
			Animation_Set_New::Import import_;

			import_.set = model_temp->animations.get();
			import_.import_skeleton = skel;
			set->imports.push_back(import_);
		}
		else if (str.cmp("source_folder")) {
			parser.read_string(str);

			Stack_String<64> folder = str.to_stack_string().c_str();
			Stack_String<256> buffer;
			Stack_String<256> directory = string_format("%s%s*", MODEL_DIRECTORY, str.to_stack_string().c_str());

			while (Files::iterate_files_in_dir(directory.c_str(), buffer.get_data(), 256)) {
				
				const Model* m = mods.find_or_load(string_format("%s%s", folder.c_str(),buffer.c_str()));

				if (m && m->animations) {
					Animation_Set_New::Import import_;
					import_.set = m->animations.get();
					import_.import_skeleton = skel;
					set->imports.push_back(import_);
				}
			}

		}
		else if (str.cmp("skeleton")) {
			parser.read_string(str);
			skel = anim_tree_man->find_skeleton(str.to_stack_string().c_str());
			if (!skel) {
				printf("Couldn't find import skeleton\n");
				return false;
			}
		}
		else {
			printf("Unknown import command %s\n", str.to_stack_string().c_str());
			return false;
		}

		parser.read_string(str);
	}

	return true;
}

const Animation_Set_New* Animation_Tree_Manager::find_set(const char* name)
{
	if (sets.find(name) != sets.end())
		return &sets[name];


	Animation_Set_New* set = &sets[name];

	DictParser parser;
	if (!parser.load_from_file(string_format("%s%s",SET_DIRECTORY, name))) {
		printf("Couldn't find animation set %s\n", name);
		return nullptr;
	}

	StringView str;
	
	parser.read_string(str);
	while (!parser.is_eof()) {

		if (str.cmp("skeleton")) {
			parser.read_string(str);

			set->src_skeleton = find_skeleton(str.to_stack_string().c_str());
			if (!set->src_skeleton) {
				printf("no skeleton found\n");
				goto had_err;
			}
		}
		else if (str.cmp("inherits")) {
			set->parent = find_set(name);
			if (!set->parent) {
				printf("couldn't find inherited set\n");
				goto had_err;
			}
		}
		else if (str.cmp("imports")) {
			if (!parser.expect_list_start()) {
				printf("Expected list\n");
				goto had_err;
			}
			parser.read_string(str); 
			while (!parser.check_list_end(str) && !parser.is_eof()) {

				if (parser.check_item_start(str)) {
					read_import(parser, str,set);
				}
				else
					ASSERT(0);
				parser.read_string(str);
			}
		}

		else if (str.cmp("remap")) {
			parser.expect_list_start();

			parser.read_string(str);
			while (!parser.is_eof() && !parser.check_list_end(str)) {
				
				parser.read_string(str);
				if (!parser.check_item_start(str))
					goto had_err;

				parser.read_string(str);
				auto key = str.to_stack_string();
				parser.read_string(str);
				auto val = str.to_stack_string();
				set->table[key.c_str()] = val.c_str();
				parser.read_string(str);

				if (!parser.check_item_end(str))
					goto had_err;

				parser.read_string(str);
			}
		}

		parser.read_string(str);
	}

	return set;

had_err:
	printf("Animation set (%s) parsing error\n", name);
	printf("%s\n", parser.get_line_str(parser.get_last_line()).to_stack_string().c_str());

	return nullptr;
}

static bool read_remap_skeleton_table(DictParser& parser, StringView& str, Model_Skeleton* skeleton)
{
	if (!skeleton->source)
		return false;

	if (!parser.expect_list_start())
		return false;

	skeleton->bone_mirror_map.resize(skeleton->source->bones.size(), -1);

	parser.read_string(str);
	while (!parser.is_eof() && !parser.check_list_end(str)) {

		if (!parser.check_item_start(str))
			return false;

		parser.read_string(str);
		auto right = str.to_stack_string();
		parser.read_string(str);
		auto left = str.to_stack_string();

		int r = skeleton->source->bone_for_name(right.c_str());
		int l = skeleton->source->bone_for_name(left.c_str());

		if (r == -1 || l == -1) {
			printf("No bone for name %s %s\n", right.c_str(), left.c_str());
			return false;
		}

		skeleton->bone_mirror_map.at(r) = l;

		parser.read_string(str);

		if (!parser.check_item_end(str))
			return false;

		parser.read_string(str);
	}

	return true;
}

static bool read_mirror_skeleton_table(DictParser& parser, StringView& str, Model_Skeleton::Remap& remap, Model_Skeleton* skel)
{

	Model_Skeleton::Remap remap_inverse;
	remap_inverse.source = skel;
	remap_inverse.source_to_skel.resize(skel->source->bones.size(), -1);

	// look up index of the source bone index to the destination index
	remap.source_to_skel.resize(remap.source->source->bones.size(),-1);

	if (!parser.expect_list_start())
		return false;
	parser.read_string(str);
	while (!parser.is_eof() && !parser.check_list_end(str)) {

		parser.read_string(str);
		if (!parser.check_item_start(str))
			return false;

		parser.read_string(str);
		auto dest = str.to_stack_string();
		parser.read_string(str);
		auto src = str.to_stack_string();

		int dest_idx = skel->source->bone_for_name(dest.c_str());
		int src_idx = remap.source->source->bone_for_name(src.c_str());

		if (src_idx == -1 || dest_idx == -1) {
			printf("No bone for name %s %s\n", dest.c_str(), src.c_str());
			return false;
		}

		remap.source_to_skel.at(src_idx) = dest_idx;
		remap_inverse.source_to_skel.at(dest_idx) = src_idx;


		parser.read_string(str);

		if (!parser.check_item_end(str))
			return false;

		parser.read_string(str);
	}

	// add the inverse of this mapping to the target skeleton
	remap.source->remaps.push_back(remap_inverse);

	return true;
}

// '{' already read
static bool read_remap_skeleton(DictParser& parser, StringView& str, Model_Skeleton* skel)
{
	Model_Skeleton::Remap remap;

	while (!parser.check_item_end(str) && !parser.is_eof()) {
		parser.read_string(str);

		if (str.cmp("source")) {
			parser.read_string(str);
			remap.source = anim_tree_man->find_skeleton(str.to_stack_string().c_str());
			if (!remap.source || remap.source->source->bones.empty()) {
				printf("couldn't find remap source %s\n", str.to_stack_string().c_str());
				return false;
			}
		}
		else if (str.cmp("table")) {
			if (!read_mirror_skeleton_table(parser, str, remap, skel))
				return false;
		}
	}

}

const Model_Skeleton* Animation_Tree_Manager::find_skeleton(const char* name) const {
	return find_skeleton(name);
}


Model_Skeleton* Animation_Tree_Manager::find_skeleton(const char* name)
{
	if (skeletons.find(name) != skeletons.end())
		return &skeletons[name];


	Model_Skeleton* skeleton = &skeletons[name];


	DictParser parser;
	if (!parser.load_from_file(string_format("%s%s", SKEL_DIRECTORY, name))) {
		printf("Couldn't find skelton %s\n", name);
		return nullptr;
	}

	StringView str;
	parser.read_string(str);
	while (!parser.is_eof()) {

		if (str.cmp("source_model")) {
			parser.read_string(str);
			const Model* m = mods.find_or_load(str.to_stack_string().c_str());
			skeleton->source = m;
		}
		else if (str.cmp("mirror")) {
			if (!read_remap_skeleton_table(parser, str, skeleton))
				goto had_err;
		}
		else if (str.cmp("remap")) {

			if (!parser.expect_list_start())
				goto had_err;

			parser.read_string(str);
			while (!parser.is_eof() && parser.check_list_end(str)) {
				
				if (!read_remap_skeleton(parser, str, skeleton))
					goto had_err;
				parser.read_string(str);
			}

		}

		parser.read_string(str);
	}

	return skeleton;

had_err:
	printf("%s\n",parser.get_line_str(parser.get_last_line()).to_stack_string().c_str());
	printf("Skeleton load had err %s\n", name);

	return nullptr;
}
