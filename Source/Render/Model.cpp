#include "Model.h"
#include "ModelManager.h"

#include "Memory.h"
#include <vector>
#include <map>
#include "glad/glad.h"
#include "glm/gtc/type_ptr.hpp"

#include "Framework/Util.h"
#include "Animation/Runtime/Animation.h"

#include "Framework/DictParser.h"
#include "Texture.h"

#include "AssetCompile/Compiliers.h"
#include "Framework/Files.h"
#include "Framework/BinaryReadWrite.h"

#include "Framework/Config.h"
#include <algorithm>

#include "Animation/SkeletonData.h"

#include "Physics/Physics2.h"

#include "Render/MaterialPublic.h"

#include <unordered_set>
#include "AssetCompile/Someutils.h" // string stuff
#include "Assets/AssetRegistry.h"

#include "Assets/AssetDatabase.h"

ModelMan g_modelMgr;

#ifdef EDITOR_BUILD
// extern IEditorTool* g_model_editor;	// defined in AssetCompile/ModelAssetEditorLocal.h
class ModelAssetMetadata : public AssetMetadata
{
public:
	ModelAssetMetadata() { extensions.push_back("cmdl"); }
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const override { return Color32(20, 125, 245); }

	virtual std::string get_type_name() const override { return "Model"; }

	// virtual IEditorTool* tool_to_edit_me() const override { return g_model_editor; }

	virtual const ClassTypeInfo* get_asset_class_type() const { return &Model::StaticType; }
};

REGISTER_ASSETMETADATA_MACRO(ModelAssetMetadata);
#endif

Model::~Model() {
	ASSERT(true); // always safe to destruct
	if (get_name() == "eng/cube.cmdl") {
		printf("");
	}
}
Model* Model::load(std::string path) {
	ASSERT(!path.empty());
	return g_assets.find<Model>(path).get();
}
Model::Model() {}

// FIXME broke
void ModelMan::compact_memory() {
#if 0
	sys_print(Debug, "ModelMan::compact_memory\n");
	std::vector<Model*> models;
	models.reserve(all_models.num_used);
	for (auto m : all_models) {
		ASSERT(m->uid != 0);
		models.push_back(m);
	}

	// [.AA.B...CCC.D.....]
	// pick last item (D)
	// for item in list:
	//		move to end
	//		wrap ptr
	// [CCC.........DAAB..]

	// theres an edge case where no models are loaded, but that will never happen (default models)

	// do indices first
	std::sort(models.begin(), models.end(), [](Model* a, Model* b)->bool
		{
			return a->merged_index_pointer < b->merged_index_pointer;
		});
	allocator.ibuffer.used_total = models.back()->data.get_num_index_bytes();
	for (int i = 0; i < models.size() - 1/* skip last */; i++) {
		int index_ptr = models[i]->merged_index_pointer;
		allocator.ibuffer.tail = index_ptr;
		//models[i]->merged_index_pointer = allocator.move_append_i_buffer(index_ptr, models[i]->data.get_num_index_bytes());

		// okay just do it this way, i guess just keep models in CPU memory
		size_t indiciesbufsize{};
		const uint8_t* const ibufferdata = models[i]->data.get_index_data(&indiciesbufsize);
		models[i]->merged_index_pointer = allocator.append_to_i_buffer(ibufferdata, indiciesbufsize);	// dont divide by sizeof(uint16_2), this is an pointer

	}

	// then verts
	std::sort(models.begin(), models.end(), [](Model* a, Model* b)->bool
		{
			return a->merged_vert_offset < b->merged_vert_offset;
		});
	allocator.vbuffer.used_total = models.back()->data.get_num_vertex_bytes();

	for (int i = 0; i < models.size() - 1/* skip last */; i++) {
		int vertex_ptr = models[i]->merged_vert_offset * sizeof(ModelVertex);
		allocator.vbuffer.tail = vertex_ptr;
		//models[i]->merged_vert_offset = allocator.move_append_v_buffer(vertex_ptr, models[i]->data.get_num_vertex_bytes());
		//models[i]->merged_vert_offset /= sizeof(ModelVertex);

		size_t vertbufsize{};
		const uint8_t* const v_bufferdata = models[i]->data.get_vertex_data(&vertbufsize);
		models[i]->merged_vert_offset = allocator.append_to_v_buffer(v_bufferdata, vertbufsize);
		models[i]->merged_vert_offset /= sizeof(ModelVertex);
	}
#endif
}

#ifdef EDITOR_BUILD
#include "AssetCompile/ModelAsset2.h"
#include <fstream>
#include "Framework/ReflectionProp.h"
#include "Framework/DictWriter.h"
#include "Framework/SerializerJson.h"

void write_model_import_settings(ModelImportSettings* mis, const std::string& savepath) {
	ASSERT(mis != nullptr);
	ASSERT(!savepath.empty());

	MakePathForGenericObj pathmaker;
	WriteSerializerBackendJson writer("write_mis", pathmaker, *mis, true);

	auto fileptr = FileSys::open_write_game(savepath);
	if (fileptr) {
		sys_print(Info, "new_import_settings_to_modeldef_data: writing new MIS JSON version %s\n", savepath.c_str());

		string out = "!json\n" + writer.get_output().dump(1);
		fileptr->write(out.data(), out.size());
	} else {
		sys_print(Error, "new_import_settings_to_modeldef_dataL Couldnt open file to write out new version of mis %s\n",
				  savepath.c_str());
	}
}
void IMPORT_MODEL_FUNC(const Cmd_Args& args) {
	ASSERT(true); // console command handler
	if (args.size() != 2) {
		sys_print(Error, "usage: IMPORT_MODEL <.glb path>");
		return;
	}

	auto savepath = strip_extension(args.at(1)) + ".mis";
	string glb_path = args.at(1);
	StringUtils::get_filename(glb_path);
	ModelImportSettings mis;
	mis.srcGlbFile = glb_path+".glb";
	write_model_import_settings(&mis, savepath);

	ModelCompilier::compile(savepath.c_str());
}
void import_model_lightmapped(const Cmd_Args& args) {
	ASSERT(true); // console command handler
	if (args.size() != 4) {
		sys_print(Error, "usage: import_model_lightmapped <.glb path> <lm x> <lm y>");
		return;
	}
	try {
		auto savepath = strip_extension(args.at(1)) + ".mis";
		ModelImportSettings mis;
		mis.srcGlbFile = args.at(1);
		mis.withLightmap = true;
		mis.lightmapSizeX = std::stoi(args.at(2));
		mis.lightmapSizeY = std::stoi(args.at(3));
		write_model_import_settings(&mis, savepath);
		ModelCompilier::compile(savepath.c_str());
	}
	catch (...) {
		sys_print(Error, "usage: import_model_lightmapped err\n");
	}
}

#endif
#include "Framework/StringUtils.h"
void export_one_model(const Model& model, const char* export_path);

void ModelMan::add_commands(ConsoleCmdGroup& group) {
	ASSERT(true); // registration, always safe
	group.add("compact_vertex_buffer", [this](const Cmd_Args&) { compact_memory(); });
	group.add("print_vertex_usage", [this](const Cmd_Args&) { print_usage(); });

#ifdef EDITOR_BUILD
	group.add("IMPORT_MODEL", IMPORT_MODEL_FUNC);
	group.add("import_model_lightmapped", import_model_lightmapped);

	group.add("export_model", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Error, "usage: export_model <model>\n");
			return;
		}
		auto mod = Model::load(args.at(1));
		if (!mod) {
			sys_print(Error, "model doesn't exist\n");
			return;
		}
		auto outPath = StringUtils::alphanumeric_hash(args.at(1)) + ".glb";
		sys_print(Info, "writing out to %s\n", outPath.c_str());
		export_one_model(*mod, outPath.c_str());
	});
	group.add("print_bones", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Error, "usage: print_bones <model>\n");
			return;
		}
		auto mod = Model::load(args.at(1));
		if (!mod) {
			sys_print(Error, "model doesn't exist\n");
			return;
		}
		if (!mod->get_skel()) {
			sys_print(Error, "model doesn't have skeleton\n");
			return;
		}
		auto skel = mod->get_skel();
		auto& bones = skel->get_all_bones();
		sys_print(Info, "numBones=%d\n", int(bones.size()));
		for (int i = 0; i < bones.size(); i++) {
			sys_print(Info, "%s = %d (parent=%d)(rt=%d)\n", bones[i].strname.c_str(), i, int(bones[i].parent),
					  int(bones[i].retarget_type));
		}
	});
	group.add("print_anims", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Error, "usage: print_bones <model>\n");
			return;
		}
		auto mod = Model::load(args.at(1));
		if (!mod) {
			sys_print(Error, "model doesn't exist\n");
			return;
		}
		if (!mod->get_skel()) {
			sys_print(Error, "model doesn't have skeleton\n");
			return;
		}
		auto skel = mod->get_skel();
		auto& clips = skel->get_all_clips();
		sys_print(Info, "numClips=%d\n", int(clips.size()));
		for (auto& [name, seq] : clips) {
			sys_print(Info, "%s (duration=%f) (additive=%d)\n", name.c_str(), seq.ptr->duration,
					  int(seq.ptr->is_additive_clip));
		}
	});
	group.add("model_info", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Error, "usage: model_info <model>\n");
			return;
		}
		auto mod = Model::load(args.at(1));
		if (!mod) {
			sys_print(Error, "model doesn't exist\n");
			return;
		}
		sys_print(Info, "hasSkel = %d\n", int(mod->get_skel() != nullptr));
		if (mod->get_skel()) {
			sys_print(Info, "numBones = %d\n", int(mod->get_skel()->get_all_bones().size()));
			sys_print(Info, "numAnims = %d\n", int(mod->get_skel()->get_num_animations()));
			sys_print(Info, "hasMirroring = %d\n", int(mod->get_skel()->has_mirroring_table()));
		}
		sys_print(Info, "materials (%d) = [\n", mod->get_num_materials());
		for (int i = 0; i < mod->get_num_materials(); i++) {
			auto mat = mod->get_material(i);
			if (mat) {
				sys_print(Info, "\t%s\n", mat->get_name().c_str());
			} else {
				sys_print(Info, "\t<null>\n");
			}
		}
		sys_print(Info, "]\n");
		sys_print(Info, "isLightmapped = %d\n", int(mod->get_lightmap_type()));
		sys_print(Info, "lightmapSize = %d %d\n", mod->get_lightmap_size().x, mod->get_lightmap_size().y);
		sys_print(Info, "hasCollision = %d\n", int(mod->get_physics_body() != nullptr));
		sys_print(Info, "numLods(%d) = [\n", mod->get_num_lods());
		for (int i = 0; i < mod->get_num_lods(); i++) {
			auto& lod = mod->get_lod(i);
			int totalV = 0;
			int totalI = 0;
			for (int p = 0; p < lod.part_count; p++) {
				totalV += mod->get_part(p + lod.part_ofs).vertex_count;
				totalI += mod->get_part(p + lod.part_ofs).element_count / MODEL_BUFFER_INDEX_TYPE_SIZE;
			}
			sys_print(Info, "\t[%d] verts=%d indicies=%d parts=%d fade=%f\n", i, totalV, totalI, lod.part_count,
					  lod.end_percentage);
		}
		sys_print(Info, "]\n");
	});

#endif
}

void ModelMan::print_usage() const {
	ASSERT(true); // always callable
	allocator.print_usage();
}

void ModelMan::set_v_attributes() {
	ASSERT(true); // stub
}

ModelMan::ModelMan() : all_models(6) {}
