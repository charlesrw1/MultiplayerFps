#include "Model.h"
#include <vector>
#include <map>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"
#include "glad/glad.h"
#include "Util.h"
#include "Animation.h"
#include "Texture.h"

static const char* const model_folder_path = "Data\\Models\\";
static std::vector<Model*> models;

// Hardcoded attribute locations for shaders
const int POSITION_LOC = 0;
const int UV_LOC = 1;
const int NORMAL_LOC = 2;
const int JOINT_LOC = 3;
const int WEIGHT_LOC = 4;
const int COLOR_LOC = 5;

static uint32_t MakeOrFindGpuBuffer(Model* m, int buf_view_index, tinygltf::Model& model, std::map<int, int>& buffer_view_to_buffer)
{
	if (buffer_view_to_buffer.find(buf_view_index) != buffer_view_to_buffer.end())
		return buffer_view_to_buffer.find(buf_view_index)->second;

	tinygltf::BufferView buffer_view = model.bufferViews.at(buf_view_index);
	tinygltf::Buffer& gltf_buffer = model.buffers.at(buffer_view.buffer);

	Model::GpuBuffer buffer;
	glGenBuffers(1, &buffer.handle);
	glBindBuffer(buffer_view.target, buffer.handle);
	glBufferData(buffer_view.target, buffer_view.byteLength, &gltf_buffer.data.at(buffer_view.byteOffset), GL_STATIC_DRAW);
	glBindBuffer(buffer_view.target, 0);
	buffer.size = buffer_view.byteLength;
	buffer.target = buffer_view.target;

	m->buffers.push_back(buffer);
	int index = m->buffers.size() - 1;

	buffer_view_to_buffer[buf_view_index] = index;
	return index;
}

void AppendGltfMeshToModel(Model* model, tinygltf::Model& inputMod, const tinygltf::Mesh& mesh, std::map<int,int>& buffer_view_to_buffer)
{
	for (int i = 0; i < mesh.primitives.size(); i++)
	{
		const tinygltf::Primitive& prim = mesh.primitives[i];

		MeshPart part;
		glGenVertexArrays(1, &part.vao);
		glBindVertexArray(part.vao);

		// index buffer
		const tinygltf::Accessor& indicies_accessor = inputMod.accessors[prim.indices];
		int index_buffer_index = MakeOrFindGpuBuffer(model, indicies_accessor.bufferView, inputMod, buffer_view_to_buffer);
		ASSERT(model->buffers[index_buffer_index].target == GL_ELEMENT_ARRAY_BUFFER);
		model->buffers[index_buffer_index].Bind();
		part.element_offset = indicies_accessor.byteOffset;
		part.element_count = indicies_accessor.count;
		part.element_type = indicies_accessor.componentType;
		part.base_vertex = 0;
		part.material_idx = prim.material;

		glCheckError();

		// Now do all the attributes
		bool found_joints_attrib = false;
		for (const auto& attrb : prim.attributes)
		{
			const tinygltf::Accessor& accessor = inputMod.accessors[attrb.second];
			int byte_stride = accessor.ByteStride(inputMod.bufferViews[accessor.bufferView]);
			int buffer_index = MakeOrFindGpuBuffer(model, accessor.bufferView, inputMod, buffer_view_to_buffer);
			ASSERT(model->buffers[buffer_index].target == GL_ARRAY_BUFFER);
			model->buffers[buffer_index].Bind();

			int location = -1;
			if (attrb.first == "POSITION") location = POSITION_LOC;
			else if (attrb.first == "TEXCOORD_0") location = UV_LOC;
			else if (attrb.first == "NORMAL") location = NORMAL_LOC;
			else if (attrb.first == "JOINTS_0") location = JOINT_LOC;
			else if (attrb.first == "WEIGHTS_0") location = WEIGHT_LOC;
			else if (attrb.first == "COLOR_0") location = COLOR_LOC;

			if (location == -1) continue;
			if (model->format == VertexFormat::Skinned && location == COLOR_LOC) {
				printf("Unused color channel on model\n");
				continue;
			}
			if (location == JOINT_LOC)
				found_joints_attrib = true;

			glEnableVertexAttribArray(location);
			if (location==JOINT_LOC) {
				glVertexAttribIPointer(location, accessor.type, accessor.componentType,
					byte_stride, (void*)accessor.byteOffset);
			}
			else {
				glVertexAttribPointer(location, accessor.type, accessor.componentType,
					accessor.normalized, byte_stride, (void*)accessor.byteOffset);
			}

			part.layout |= (1 << location);

			glCheckError();
		}

		if (!found_joints_attrib && model->format == VertexFormat::Skinned) {
			printf("Conflicting vertex types in model, incoming errors\n");
		}

		glBindVertexArray(0);

		model->parts.push_back(part);

		glCheckError();
	}
}

static Texture* LoadGltfImage(tinygltf::Image& i, tinygltf::Model& scene)
{
	tinygltf::BufferView& bv = scene.bufferViews[i.bufferView];
	tinygltf::Buffer& b = scene.buffers[bv.buffer];
	ASSERT(bv.byteStride == 0);

	return CreateTextureFromImgFormat(&b.data.at(bv.byteOffset), bv.byteLength, i.name);
}

static void LoadMaterials(Model* m, tinygltf::Model& scene)
{
	for (int matidx = 0; matidx < scene.materials.size(); matidx++) {
		tinygltf::Material& mat = scene.materials[matidx];
		MeshMaterial mymat;
		if (mat.pbrMetallicRoughness.baseColorTexture.index != -1) {
			mymat.t1 = LoadGltfImage(scene.images.at(mat.pbrMetallicRoughness.baseColorTexture.index), scene);
		}

		m->materials.push_back(mymat);
		
	}
}

static void RecursiveAddSkeleton(std::map<std::string, int>& bone_to_index, tinygltf::Model& scene, Model* m, tinygltf::Node& node)
{
	if (bone_to_index.find(node.name) != bone_to_index.end())
	{
		int my_index = bone_to_index[node.name];
		for (int i = 0; i < node.children.size(); i++) {
			tinygltf::Node& child = scene.nodes[node.children[i]];
			if (bone_to_index.find(child.name) != bone_to_index.end()) {
				m->bones[bone_to_index[child.name]].parent = my_index;
			}
		}
	}
	for (int i = 0; i < node.children.size(); i++) {
		RecursiveAddSkeleton(bone_to_index, scene, m, scene.nodes[node.children[i]]);
	}
}

static void LoadGltfSkeleton(tinygltf::Model& scene, Model* model, tinygltf::Skin& skin)
{
	std::map<std::string, int> bone_to_index;
	ASSERT(skin.inverseBindMatrices != -1);
	tinygltf::Accessor& invbind_acc = scene.accessors[skin.inverseBindMatrices];
	tinygltf::BufferView& invbind_bv = scene.bufferViews[invbind_acc.bufferView];
	for (int i = 0; i < skin.joints.size(); i++) {
		tinygltf::Node& node = scene.nodes[skin.joints[i]];
		Bone b;
		b.parent = -1;
		float* start =(float*)(&scene.buffers[invbind_bv.buffer].data.at(invbind_bv.byteOffset) + sizeof(float)*16*i);
		b.invposematrix = glm::mat4(start[0],start[1],start[2],start[3],
									start[4],start[5],start[6],start[7],
									start[8],start[9],start[10],start[11],
									start[12],start[13],start[14],start[15]);
		b.posematrix = glm::inverse(glm::mat4(b.invposematrix));
		
		b.name_table_ofs = model->bone_string_table.size();
		for (auto c : node.name)
			model->bone_string_table.push_back(c);
		model->bone_string_table.push_back('\0');

		// needed when animations dont have any keyframes, cause gltf exports nothing when its euler angles ???
		b.rot = glm::quat_cast(glm::mat4(b.posematrix));

		bone_to_index.insert({ node.name, model->bones.size() });
		model->bones.push_back(b);
	}
	RecursiveAddSkeleton(bone_to_index, scene, model, scene.nodes[skin.joints[0]]);
}

static Animation_Set* LoadGltfAnimations(tinygltf::Model& scene, tinygltf::Skin& skin)
{
	Animation_Set* set = new Animation_Set;
	std::map<int, int> node_to_index;
	for (int i = 0; i < skin.joints.size(); i++) {
		node_to_index[skin.joints[i]] = i;
	}
	set->num_channels = skin.joints.size();
	for (int a = 0; a < scene.animations.size(); a++)
	{
		tinygltf::Animation& gltf_anim = scene.animations[a];

		Animation my_anim{};
		my_anim.name = std::move(gltf_anim.name);
		my_anim.channel_offset = set->channels.size();
		my_anim.pos_offset = set->positions.size();
		my_anim.rot_offset = set->rotations.size();
		my_anim.scale_offset = set->scales.size();
		set->channels.resize(set->channels.size() + skin.joints.size());
		for (int c = 0; c < gltf_anim.channels.size(); c++) {
			tinygltf::AnimationChannel& gltf_channel = gltf_anim.channels[c];
			int channel_idx = node_to_index[gltf_channel.target_node];
			AnimChannel& my_channel = set->channels[my_anim.channel_offset + channel_idx];
			
			int type = -1;
			if (gltf_channel.target_path == "translation") type = 0;
			else if (gltf_channel.target_path == "rotation") type = 1;
			else if (gltf_channel.target_path == "scale") type = 2;
			else continue;

			tinygltf::AnimationSampler& sampler = gltf_anim.samplers[gltf_channel.sampler];
			tinygltf::Accessor& timevals = scene.accessors[sampler.input];
			tinygltf::Accessor& vals = scene.accessors[sampler.output];
			ASSERT(timevals.count == vals.count);
			tinygltf::BufferView& time_bv = scene.bufferViews[timevals.bufferView];
			tinygltf::BufferView& val_bv = scene.bufferViews[vals.bufferView];
			ASSERT(time_bv.buffer == val_bv.buffer);
			tinygltf::Buffer& buffer = scene.buffers[time_bv.buffer];
			ASSERT(timevals.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);// just make life easier
			ASSERT(time_bv.byteStride == 0);

			my_anim.total_duration = glm::max(my_anim.total_duration, (float)timevals.maxValues.at(0));

			float* time_buffer = (float*)(&buffer.data.at(time_bv.byteOffset));
			if (type == 0) {
				my_channel.pos_start = set->positions.size();
				ASSERT(vals.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && vals.type == TINYGLTF_TYPE_VEC3);
				ASSERT(val_bv.byteStride == 0);
				glm::vec3* pos_buffer = (glm::vec3*)(&buffer.data.at(val_bv.byteOffset));
				for (int t = 0; t < timevals.count; t++) {
					PosKeyframe pkf;
					pkf.time = time_buffer[t];
					pkf.val = pos_buffer[t];
					set->positions.push_back(pkf);
				}
				my_channel.num_positions = set->positions.size() - my_channel.pos_start;

			}
			else if (type == 1) {
				my_channel.rot_start = set->rotations.size();
				ASSERT(vals.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && vals.type == TINYGLTF_TYPE_VEC4);
				ASSERT(val_bv.byteStride == 0);
				glm::quat* rot_buffer = (glm::quat*)(&buffer.data.at(val_bv.byteOffset));
				for (int t = 0; t < timevals.count; t++) {
					RotKeyframe rkf;
					rkf.time = time_buffer[t];
					rkf.val = rot_buffer[t];
					set->rotations.push_back(rkf);
				}
				my_channel.num_rotations = set->rotations.size() - my_channel.rot_start;
			}
			else if (type == 2) {
				my_channel.scale_start = set->scales.size();
				ASSERT(vals.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && vals.type == TINYGLTF_TYPE_VEC3);
				ASSERT(val_bv.byteStride == 0);
				glm::vec3* scale_buffer = (glm::vec3*)(&buffer.data.at(val_bv.byteOffset));
				for (int t = 0; t < timevals.count; t++) {
					ScaleKeyframe skf;
					skf.time = time_buffer[t];
					skf.val = scale_buffer[t];
					set->scales.push_back(skf);
				}
				my_channel.num_scales = set->scales.size() - my_channel.scale_start;
			}
		}

		my_anim.num_pos = set->positions.size() - my_anim.pos_offset;
		my_anim.num_rot = set->rotations.size() - my_anim.rot_offset;
		my_anim.num_scale = set->scales.size() - my_anim.scale_offset;
		my_anim.fps = 1.0;
		set->clips.push_back(my_anim);
	}

	return set;
}

static bool DoLoadGltfModel(const std::string& filepath, Model* model)
{
	tinygltf::Model scene;
	tinygltf::TinyGLTF loader;
	std::string errStr;
	std::string warnStr;
	bool res = loader.LoadBinaryFromFile(&scene, &errStr, &warnStr, filepath);
	if (!res) {
		printf("Couldn't load gltf model: %s\n", errStr.c_str());
		return false;
	}

	if (scene.skins.size() >= 1)
		LoadGltfSkeleton(scene, model, scene.skins[0]);
	if (scene.animations.size() >= 1 && scene.skins.size() >= 1) {
		Animation_Set* set = LoadGltfAnimations(scene, scene.skins[0]);
		model->animations = std::unique_ptr<Animation_Set>(set);
	}

	std::map<int, int> buf_view_to_buffers;
	for (int i = 0; i < scene.meshes.size(); i++) {
		AppendGltfMeshToModel(model, scene, scene.meshes.at(i),buf_view_to_buffers);
	}
	LoadMaterials(model, scene);


	return res;
}


void Model::GpuBuffer::Bind()
{
	glBindBuffer(target, handle);
}

int Model::BoneForName(const char* name)
{
	for (int i = 0; i < bones.size(); i++) {
		if (strcmp(&bone_string_table.at(bones[i].name_table_ofs), name) == 0)
			return i;
	}
	return -1;
}

void FreeLoadedModels()
{
	for (int i = 0; i < models.size(); i++) {
		Model* m = models[i];
		printf("Freeing model: %s\n", m->name.c_str());
		for (int p = 0; p < m->parts.size(); p++) {
			glDeleteVertexArrays(1, &m->parts[p].vao);
		}
		for (int b = 0; b < m->buffers.size(); b++) {
			glDeleteBuffers(1, &m->buffers[b].handle);
		}

		delete m;
	}
	models.clear();
}
void ReloadModel(Model* m)
{
	std::string path;
	path.reserve(256);
	path += model_folder_path;
	path += m->name;

	for (int p = 0; p < m->parts.size(); p++) {
		glDeleteVertexArrays(1, &m->parts[p].vao);
	}
	for (int b = 0; b < m->buffers.size(); b++) {
		glDeleteBuffers(1, &m->buffers[b].handle);
	}

	*m = Model{};
	bool res = DoLoadGltfModel(path, m);
}
Model* FindOrLoadModel(const char* filename)
{
	for (int i = 0; i < models.size(); i++) {
		if (models[i]->name == filename)
			return models[i];
	}

	std::string path;
	path.reserve(256);
	path += model_folder_path;
	path += filename;

	Model* model = new Model;
	bool res = DoLoadGltfModel(path, model);
	if (!res) {
		delete model;
		return nullptr;
	}

	model->name = filename;
	models.push_back(model);

	return model;

}
