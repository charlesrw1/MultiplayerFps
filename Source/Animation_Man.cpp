#include "AnimationTreePublic.h"

#include "DictParser.h"

#include "Model.h"
#include "Util.h"

static const char* GRAPH_DIRECTORY = "Data/Animations/Graphs/";
static const char* SKEL_DIRECTORY = "Data/Animations/Skels/";
static const char* MODEL_DIRECTORY = "Data/Models/";
static const char* NOTIFY_DIRECTORY = "Data/Animations/Notify/";
static const char* SET_DIRECTORY = "Data/Animations/Sets/";

static bool check_imports(Animation_Set_New* set, Model_Skeleton* skeleton)
{
	ASSERT(skeleton != nullptr && skeleton == set->src_skeleton);
	

	for (int i = 0; i < set->imports.size(); i++) {
		auto& import_ = set->imports[i];
		Model_Skeleton* import_skel = import_.import_skeleton;
		const Model* mod = import_.mod;

		if (mod != import_skel->source) {
			if (mod->bones.size() != import_skel->source->bones.size()) {
				printf("BONE MISMATCH!! %s %s\n", mod->name.c_str(), import_skel->source->name.c_str());
				return false;
			}
		}

		if (mod == skeleton->source)
			continue;

		bool needs_automatic_remap = true;
		if (import_skel) {
			int remap = skeleton->find_remap(import_skel);

			if (remap != -1 && skeleton->remaps[remap].skel_to_source.size() != skeleton->source->bones.size()) {
				printf("Manual remap but not created correctly?\n");
				ASSERT(0);
				continue;
			}

			if (remap == -1) {
				std::vector<int> remap;

				for (int i = 0; i < skeleton->source->bones.size(); i++) {
					int other_index = mod->bone_for_name(skeleton->source->bones[i].name.c_str());
					remap.push_back(other_index);
				}
				int i = 0;
				for (; i < remap.size(); i++) {
					if (remap[i] != i) break;
				}
				if (i != remap.size()) {
					Model_Skeleton::Remap r;
					r.skel_to_source = std::move(remap);
					r.source = import_skel;

					printf("Created unique remap between %s and %s\n", skeleton->source->name.c_str(), mod->name.c_str());

					skeleton->remaps.push_back(r);
				}
				else {
					Model_Skeleton::Remap r;
					r.source = import_skel;	// no remap needed
					skeleton->remaps.push_back(r);
				}
			}
		}
		else
			return false;
	}
	return true;
}

// expected that '{' was read
static bool read_import(DictParser& parser, StringView& str,Animation_Set_New* set)
{

	const Model* model_temp = nullptr;


	Model_Skeleton* skel = nullptr;

	parser.read_string(str);
	while (!parser.check_item_end(str) && !parser.is_eof()) {
		if (str.cmp("source")) {

			if (!skel)
				return false;

			parser.read_string(str);
			model_temp = mods.find_or_load(str.to_stack_string().c_str());
			if (!model_temp || !model_temp->animations) {
				printf("Couldn't find import model\n");
				return false;
			}
			Animation_Set_New::Import import_;

			import_.mod = model_temp;
			import_.import_skeleton = skel;
			set->imports.push_back(import_);
		}
		else if (str.cmp("source_folder")) {

			if (!skel)
				return false;

			parser.read_string(str);

			Stack_String<64> folder = str.to_stack_string().c_str();
			Stack_String<256> buffer;
			Stack_String<256> directory = string_format("%s%s*", MODEL_DIRECTORY, str.to_stack_string().c_str());

			while (Files::iterate_files_in_dir(directory.c_str(), buffer.get_data(), 256)) {
				
				const Model* m = mods.find_or_load(string_format("%s%s", folder.c_str(),buffer.c_str()));

				if (m && m->animations) {
					Animation_Set_New::Import import_;
					import_.mod = m;
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
	if (sets.find(name) != sets.end()) {
		Animation_Set_New* set = &sets[name];
		return (set->data_is_valid) ? set : nullptr;
	}

	Animation_Set_New* set = &sets[name];

	DictParser parser;
	if (!parser.load_from_file(string_format("%s%s",SET_DIRECTORY, name))) {
		printf("Couldn't find animation set %s\n", name);
		return nullptr;
	}

	StringView str;

	Model_Skeleton* src_skel = nullptr;
	
	parser.read_string(str);
	while (!parser.is_eof()) {

		if (str.cmp("skeleton")) {
			parser.read_string(str);
			src_skel = find_skeleton(str.to_stack_string().c_str());
			set->src_skeleton = src_skel;
			if (!set->src_skeleton) {
				printf("no skeleton found\n");
				goto had_err;
			}
		}
		else if (str.cmp("inherits")) {
			const Animation_Set_New* parent_set = find_set(name);
			if (!parent_set) {
				printf("couldn't find inherited set\n");
				goto had_err;
			}
			for (auto& key : parent_set->table) {
				set->table[key.first] = key.second;
			}
			for (int i = 0; i < parent_set->imports.size(); i++) {
				set->imports.push_back(parent_set->imports[i]);
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

	if (!set->src_skeleton || set->imports.empty()) {
		printf("Animation set is not valid %s\n", name);
		return nullptr;
	}

	set->data_is_valid = true;

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
	remap_inverse.skel_to_source.resize(remap.source->source->bones.size(), -1);

	// look up index of the source bone index to the destination index
	remap.skel_to_source.resize(skel->source->bones.size(),-1);

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

		remap.skel_to_source.at(dest_idx) = src_idx;
		remap_inverse.skel_to_source.at(src_idx) = dest_idx;


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
	if (skeletons.find(name) != skeletons.end()) {
		Model_Skeleton* sk = &skeletons[name];
		return (sk->data_is_valid) ? sk : nullptr;
	}

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

	if (!skeleton->source) {
		printf("skeleton doesn't have a source model for bones\n");
		return nullptr;
	}
	skeleton->data_is_valid = true;

	return skeleton;

had_err:
	printf("%s\n",parser.get_line_str(parser.get_last_line()).to_stack_string().c_str());
	printf("Skeleton load had err %s\n", name);

	return nullptr;
}

#include "AnimationTreeLocal.h"

void Animation_Tree_Manager::on_new_animation(Model* m, int index)
{
	Animation_Link& link = notifies[m->animations->clips[index].name];
	link.model = m;
	link.index = index;
	m->animations->clips.at(index).notify = &link.list;
}

static AnimationNotifyType str_to_type(StringView view)
{
	if (view.cmp("SOUND")) {
		return AnimationNotifyType::SOUND;
	}

	else if (view.cmp("EFFECT")) {
		return AnimationNotifyType::EFFECT;
	}

	else if (view.cmp("FOOTSTEP_LEFT")) {
		return AnimationNotifyType::FOOTSTEP_LEFT;
	}

	else if (view.cmp("FOOTSTEP_RIGHT")) {
		return AnimationNotifyType::FOOTSTEP_RIGHT;
	}

	printf("UNKNOWN AnimationNotifyType: %s\n", view.to_stack_string().c_str());
	return AnimationNotifyType::FOOTSTEP_RIGHT;
}
#include <algorithm>
void Animation_Tree_Manager::parse_notify_file(DictParser& parser) {

	StringView str;

	using AND = Animation_Notify_Def;
	std::vector<AND> load_buf;

	parser.read_string(str);
	while (!parser.is_eof())
	{
		if (!parser.check_item_start(str))
			return;

		parser.read_string(str);
		if (!str.cmp("anim"))
			return;
		parser.read_string(str);

		auto& link = notifies[std::string(str.to_stack_string().c_str())];

		parser.read_string(str);
		while (!parser.check_item_end(str) && !parser.is_eof()) {
			
			if (str.cmp("events")) {
				parser.expect_list_start();

				parser.read_string(str);
				while (!parser.check_list_end(str) && !parser.is_eof()) {

					if (parser.check_item_start(str)) {
						
						AND def;

						parser.read_string(str);
						while (!parser.check_item_end(str) && !parser.is_eof()) {

							if (str.cmp("event")) {

								parser.read_string(str);
								def.type = str_to_type(str);
							}
							else if (str.cmp("params")) {
								if (!parser.expect_list_start())
									return;

								parser.read_string(str);
								while (!parser.check_list_end(str) && !parser.is_eof()) {

									if (def.param_count < 4) {

										def.param_count++;

										char* c =(char*)event_arena.alloc_bottom(str.str_len+1);
										memcpy(c, str.str_start, str.str_len+1);
										c[str.str_len] = 0;
										def.params[def.param_count - 1] = c;
									}

									parser.read_string(str);
								}
							}
							else if (str.cmp("begin")) {
								parser.read_string(str);

								if (str.cmp("END")) {
									def.start = 1000.f;
								}
								else {
									def.start = atof(str.to_stack_string().c_str());
								}
							}
							else if (str.cmp("stop")) {

								parser.read_string(str);

								if (str.cmp("END")) {
									def.end = 1000.f;
								}
								else {
									def.end = atof(str.to_stack_string().c_str());
								}
							}


							parser.read_string(str);
						}
						
						load_buf.push_back(def);
					}

					parser.read_string(str);
				}

			}
			else {
				printf("unknown notify command %s\n", str.to_stack_string().c_str());
				return;
			}

			parser.read_string(str);
		}

		if (load_buf.size() > 0) {
			std::sort(load_buf.begin(), load_buf.end(), [](const AND& first, const AND& other) { return first.start < other.end; });
			link.list.defs = (AND*)event_arena.alloc_bottom(sizeof(AND) * load_buf.size());
			link.list.count = load_buf.size();

			memcpy(link.list.defs, load_buf.data(), sizeof(AND) * load_buf.size());
		}
		load_buf.clear();


		parser.read_string(str);
	}
}

void Animation_Tree_Manager::load_notifies()
{
	Stack_String<64> dir = string_format("%s*", NOTIFY_DIRECTORY);
	Stack_String<128> buffer;
	while (Files::iterate_files_in_dir(dir.c_str(), buffer.get_data(), 128)) {

		DictParser parser;
		bool good = parser.load_from_file(string_format("%s%s", NOTIFY_DIRECTORY, buffer.c_str()));
		if (!good)
			continue;
		parse_notify_file(parser);
	}
}

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
	event_arena.init("ANIMATION NOTIFY", 32'000);
	load_notifies();
}
