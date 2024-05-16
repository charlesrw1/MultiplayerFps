#include "AnimationUtil.h"

#include "Debug.h"

static glm::mat4 debug_animation_transform = glm::mat4(1);

void Animation_Debug::set_local_to_world(glm::mat4 transform) {
	debug_animation_transform = transform;
}
void Animation_Debug::push_line(const glm::mat4& transform_me, const glm::mat4& transform_parent, bool has_parent) {

	const auto& transform = debug_animation_transform;
	vec3 org = transform * transform_me[3];
	Color32 colors[] = { COLOR_RED,COLOR_GREEN,COLOR_BLUE };
	for (int i = 0; i < 3; i++) {
		vec3 dir = glm::mat3(transform) * transform_me[i];
		dir = normalize(dir);
		Debug::add_line(org, org + dir * 0.1f, colors[i], -1.f, false);
	}

	if (has_parent) {
		vec3 parent_org = transform * transform_parent[3];
		Debug::add_line(org, parent_org, COLOR_PINK, -1.f, false);
	}

}
void Animation_Debug::push_sphere(const glm::vec3& p, float radius) {
	glm::vec3 org = debug_animation_transform * glm::vec4(p, 1.0);
	Debug::add_sphere(org, radius, COLOR_GREEN,-1.f,false);
}


void util_subtract(int bonecount, const Pose& reference, Pose& source)
{
	for (int i = 0; i < bonecount; i++) {
		source.pos[i] = source.pos[i] - reference.pos[i];
		source.q[i] = source.q[i] - reference.q[i];
	}

}

// b = lerp(a,b,f)
void util_blend(int bonecount, const Pose& a, Pose& b, float factor)
{
	for (int i = 0; i < bonecount; i++) {
		b.q[i] = glm::slerp(b.q[i], a.q[i], factor);
		b.q[i] = glm::normalize(b.q[i]);
		b.pos[i] = glm::mix(b.pos[i], a.pos[i], factor);
	}
}
void util_blend_with_mask(int bonecount, const Pose& a, Pose& b, float factor, const std::vector<float>& mask)
{
	ASSERT(mask.size() >= bonecount);
	for (int i = 0; i < bonecount; i++) {
		b.q[i] = glm::slerp(b.q[i], a.q[i], factor*mask[i]);
		b.q[i] = glm::normalize(b.q[i]);
		b.pos[i] = glm::mix(b.pos[i], a.pos[i], factor*mask[i]);
	}
}

static glm::quat quat_delta(const glm::quat& from, const glm::quat& to)
{
	return to * glm::inverse(from);
}
static glm::quat quat_blend_additive(const glm::quat& a, const glm::quat& b, float t)
{
	glm::quat target = b * a;
	return glm::slerp(a, target, t);
}


void util_global_bl34234end(const Model_Skeleton* skel, const Pose* a, Pose* b, float factor, const std::vector<float>& mask)
{
	const auto& bone_vec = skel->source->bones;
	const int bonecount = bone_vec.size();

	glm::mat4* matricies1 = new glm::mat4[256];
	glm::mat4* matricies2 = new glm::mat4[256];


	util_localspace_to_meshspace_ptr(*a, matricies1, skel->source);
	util_localspace_to_meshspace_ptr(*b, matricies2, skel->source);







	delete[] matricies1;
	delete[] matricies2;

	// now do a blend between base and global blened result
	//util_blend(bonecount, *a, *b, 1.0 -factor);
}

#include "Framework/Config.h"

static glm::quat forward_dir = glm::quat(0.0,0,0,1);
static bool face_foward = true;
static bool apply_to_local = false;
static bool apply_to_all = false;
#include "imgui.h"
#include "Game_Engine.h"
void menu1235()
{
	if(ImGui::Begin("abc")) {
		ImGui::SliderFloat4("dir", &forward_dir.x,-1,1);
		ImGui::Checkbox("ff", &face_foward);
		ImGui::Checkbox("apply_to_local", &apply_to_local);
		ImGui::Checkbox("apply_to_all", &apply_to_all);

		forward_dir = glm::normalize(forward_dir);
	} ImGui::End();

	glm::mat4 transform = glm::mat4_cast(forward_dir);
	Debug::add_line(vec3(0.f), transform[0], COLOR_RED, -1.f, false);
	Debug::add_line(vec3(0.f), transform[1], COLOR_GREEN, -1.f, false);
	Debug::add_line(vec3(0.f), transform[2], COLOR_BLUE, -1.f, false);


}

static AddToDebugMenu aeasdf("meun", menu1235);


static void draw_skeleton_util_debug(const glm::mat4* bones,const Model* model, int count, float line_len, const glm::mat4& transform)
{

	for (int index = 0; index < count; index++) {
		vec3 org = transform * bones[index][3];
		Color32 colors[] = { COLOR_RED,COLOR_GREEN,COLOR_BLUE };
		for (int i = 0; i < 3; i++) {
			vec3 dir = glm::mat3(transform) * bones[index][i];
			dir = normalize(dir);
			Debug::add_line(org, org + dir * line_len, colors[i], -1.f, false);
		}

		if (model->bones[index].parent != -1) {
			vec3 parent_org = transform * bones[model->bones[index].parent][3];
			Debug::add_line(org, parent_org, COLOR_PINK, -1.f, false);
		}
	}
}

void util_meshspace_to_localspace(const glm::mat4* mesh, const Model* mod, Pose* out)
{
	const int count = mod->bones.size();
	for (int i = 0; i < count; i++) {
		auto& bone = mod->bones[i];
		int parent = bone.parent;

		glm::mat4 matrix = mesh[i];
		if (parent != -1) {
			matrix =  glm::inverse(mesh[parent])*matrix;
		}

		out->pos[i] = matrix[3];
		out->q[i] = glm::quat_cast(matrix);
	}
}

void util_localspace_to_meshspace_ptr_2(const Pose& local, glm::mat4* out_bone_matricies, const Model* model)
{
	for (int i = 0; i < model->bones.size(); i++)
	{
		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);

		if (model->bones[i].parent == -1) {
			out_bone_matricies[i] = matrix;
		}
		else {
			assert(model->bones[i].parent < model->bones.size());
			out_bone_matricies[i] = out_bone_matricies[model->bones[i].parent] * matrix;
		}
	}
	//for (int i = 0; i < model->bones.size(); i++)
	//	out_bone_matricies[i] =  out_bone_matricies[i];
}

void util_global_blend(const Model_Skeleton* skel, const Pose* a,  Pose* b, float factor, const std::vector<float>& mask)
{
	const auto& bone_vec = skel->source->bones;
	const int bonecount = bone_vec.size();

	std::vector<glm::mat4> globalspace_base(bonecount);

	//for (int i = 0; i < bonecount; i++)
	//{
	//	glm::mat4x4 matrix = glm::mat4_cast(glm::slerp(a->q[i],b->q[i],mask[i]));
	//	matrix[3] = glm::vec4(glm::mix(a->pos[i],b->pos[i],mask[i]),1.f);
	//
	//	if (bone_vec[i].parent == -1) {
	//		globalspace_base[i] = matrix;
	//	}
	//	else {
	//		//assert(model->bones[i].parent < model->bones.size());
	//		globalspace_base[i] = globalspace_base[bone_vec[i].parent] * matrix;
	//	}
	//}

	util_localspace_to_meshspace_ptr_2(*a, globalspace_base.data(), skel->source);


	std::vector<glm::mat4> globalspace_layer(bonecount);
	util_localspace_to_meshspace_ptr_2(*b, globalspace_layer.data(), skel->source);

	std::vector<glm::quat> globalspace_rotations(bonecount);

	for (int j = 0; j < bonecount; j++) {
		glm::quat base = glm::quat_cast(globalspace_base[j]);
		glm::quat layer = glm::quat_cast(globalspace_layer[j]);
		glm::quat global = glm::slerp(base, layer, mask[j]);	// meshspace 
		globalspace_rotations[j] = global;
		//glm::vec4 t = globalspace_layer[j][3];
		//globalspace_base[j] = glm::mat4_cast(layer);
		//globalspace_base[j][3] = t;
#if 1
		int parent = bone_vec[j].parent;
		if (parent == -1) {
			b->q[j]=global;
		}
		else {
			glm::mat3 matrix = glm::mat3_cast(global);
			glm::mat3 parent_mat = glm::mat3_cast(globalspace_rotations[parent]);
			matrix = glm::inverse(parent_mat) * matrix;
			b->q[j] = glm::quat_cast(matrix);
		}

		b->pos[j] = a->pos[j];
#endif
	}





	//util_meshspace_to_localspace(globalspace_base.data(), skel->source, b);
	return;




	return;


	InlineVec<glm::quat, 128> baserotations;
	InlineVec<glm::quat, 128> layerrotations;
	InlineVec<glm::quat, 128> output_rots;
	InlineVec<bool, 128> booleans;
	baserotations.resize(bonecount);
	layerrotations.resize(bonecount);
	output_rots.resize(bonecount);
	booleans.resize(bonecount, false);

	Pose* out = Pose_Pool::get().alloc(1);


	for (int i = 0; i < bonecount; i++) {
		const int parent = bone_vec[i].parent;
		if (parent == -1) {
			baserotations[i] = a->q[i];

			layerrotations[i] = b->q[i];
		}
		else {
			baserotations[i] = a->q[i] * baserotations[parent];

			layerrotations[i] = b->q[i] * layerrotations[parent];
			
		}

	}

	for (int i = 0; i < bonecount; i++) {
		const int parent = bone_vec[i].parent;
		const float weight = mask[i];

		out->pos[i] = glm::mix(a->pos[i], b->pos[i], weight);
		
		output_rots[i] = glm::slerp(baserotations[i], layerrotations[i], weight);

		//if((weight > 0.5||apply_to_all) && face_foward)
		//	output_rots[i] = forward_dir;

		booleans[i] = true;
		if (parent != -1) {
			ASSERT(booleans[parent]);

			//glm::mat4 transform = glm::mat4_cast(output_rots[parent]);
			//transform[3] = glm::vec4(b->pos[parent],1.f);
			//auto inv = glm::inverse(transform);
			//
			//glm::mat4 transform2 = glm::mat4_cast(output_rots[i]);
			//transform2[3] = glm::vec4(b->pos[i], 1.f);
			//
			//glm::quat localrot = glm::quat_cast(transform2 * inv);

			glm::quat localrot = quat_delta(output_rots[parent], output_rots[i]);
			
			//if ((weight > 0.5 || apply_to_all)&& apply_to_local)
			//	localrot = forward_dir;
			
			out->q[i] = localrot;
		}
		else {
			out->q[i] = output_rots[i];
		}

	}


#if 0
	glm::mat4* bone_to_world = new glm::mat4[256];

	for (int i = 0; i < bonecount; i++) {
		const int parent = bone_vec[i].parent;
		const float weight = mask[i];


		b->pos[i] = glm::mix(a->pos[i], b->pos[i], weight);
		output_rots[i] = glm::slerp(baserotations[i], layerrotations[i], weight);

		bone_to_world[i] =  glm::mat4_cast(output_rots[i]);
		bone_to_world[i][3] = glm::vec4(b->pos[i], 1.0);

		if (parent != -1) {

			auto inv = glm::inverse(bone_to_world[parent]);

			auto local = bone_to_world[i]*inv;

			b->q[i] = glm::quat_cast(local);
		}
		else {
			b->q[i] = glm::quat_cast(bone_to_world[i]);
		}

	}
	delete[] bone_to_world;
#endif


	* b = *out;

	Pose_Pool::get().free(1);

	// now do a blend between base and global blened result
	//util_blend(bonecount, *a, *b, 1.0 -factor);
}

// base = lerp(base,base+additive,f)
void util_add(int bonecount, const Pose& additive, Pose& base, float fac)
{
	for (int i = 0; i < bonecount; i++) {
		base.pos[i] = glm::mix(base.pos[i], base.pos[i] + additive.pos[i], fac);
		base.q[i] = glm::slerp(base.q[i], base.q[i] + additive.q[i], fac);
		base.q[i] = glm::normalize(base.q[i]);
	}
}

void util_twobone_ik(const vec3& a, const vec3& b, const vec3& c, const vec3& target, const vec3& pole_vector, const glm::quat& a_global_rotation, const glm::quat& b_global_rotation, glm::quat& a_local_rotation, glm::quat& b_local_rotation)
{
	float eps = 0.01;
	float len_ab = length(b - a);
	float len_cb = length(b - c);
	float len_at = glm::clamp(length(target - a), eps, len_ab + len_cb - eps);

	// Interior angles of a and b
	float a_interior_angle = acos(dot(normalize(c - a), normalize(b - a)));
	float b_interior_angle = acos(dot(normalize(a - b), normalize(c - b)));
	vec3 c_a = c - a;
	vec3 target_a = normalize(target - a);
	float dot_c_a_t_a = dot(normalize(c_a), target_a);
	float c_interior_angle = acos(glm::clamp(dot_c_a_t_a, -0.9999999f, 0.9999999f));

	// Law of cosines to get the desired angles of the triangle
	float a_desired_angle = acos(LawOfCosines(len_cb, len_ab, len_at));
	float b_desired_angle = acos(LawOfCosines(len_at, len_ab, len_cb));

	// Axis to rotate around
	vec3 d = b_global_rotation * pole_vector;
	//vec3 axis0 =   normalize(cross(c - a, d));
	vec3 axis0 = normalize(cross(c - a, b - a));
	vec3 t_a = target - a;
	vec3 cross_c_a_ta = cross(c_a, t_a);
	vec3 axis1 = normalize(cross_c_a_ta);
	glm::quat rot0 = glm::angleAxis(a_desired_angle - a_interior_angle, glm::inverse(a_global_rotation) * axis0);
	glm::quat rot1 = glm::angleAxis(b_desired_angle - b_interior_angle, glm::inverse(b_global_rotation) * axis0);
	glm::quat rot2 = glm::angleAxis(c_interior_angle, glm::inverse(a_global_rotation) * axis1);

	a_local_rotation = a_local_rotation * (rot0 * rot2);
	b_local_rotation = b_local_rotation * rot1;
}

// y2 = blend( blend(x1,x2,fac.x), blend(y1,y2,fac.x), fac.y)
void util_bilinear_blend(int bonecount, const Pose& x1, Pose& x2, const Pose& y1, Pose& y2, glm::vec2 fac)
{
	util_blend(bonecount, x1, x2, fac.x);
	util_blend(bonecount, y1, y2, fac.x);
	util_blend(bonecount, x2, y2, fac.y);
}

void util_calc_rotations(const Animation_Set* set, float curframe, int clip_index, const Model* model, const std::vector<int>* remap_indicies, Pose& pose)
{
	for (int dest_idx = 0; dest_idx < set->num_channels; dest_idx++) {
		int src_idx = (remap_indicies) ? (*remap_indicies)[dest_idx] : dest_idx;

		if (src_idx == -1) {
			pose.pos[dest_idx] = model->bones.at(dest_idx).localtransform[3];
			pose.q[dest_idx] = model->bones.at(dest_idx).rot;
			pose.q[dest_idx] = glm::normalize(pose.q[dest_idx]);
			continue;
		}


		int pos_idx = set->FirstPositionKeyframe(curframe, src_idx, clip_index);
		int rot_idx = set->FirstRotationKeyframe(curframe, src_idx, clip_index);

		vec3 interp_pos{};
		if (pos_idx == -1)
			interp_pos = model->bones.at(src_idx).localtransform[3];
		else if (pos_idx == set->GetChannel(clip_index, src_idx).num_positions - 1)
			interp_pos = set->GetPos(src_idx, pos_idx, clip_index).val;
		else {
			int index0 = pos_idx;
			int index1 = pos_idx + 1;
			float t0 = set->GetPos(src_idx, index0, clip_index).time;
			float t1 = set->GetPos(src_idx, index1, clip_index).time;
			if (index0 == 0)t0 = 0.f;
			//float scale = MidLerp(clip.GetPos(i, index0).time, clip.GetPos(i, index1).time, curframe);
			//interp_pos = glm::mix(clip.GetPos(i, index0).val, clip.GetPos(i, index1).val, scale);
			float scale = MidLerp(t0, t1, curframe);
			assert(scale >= 0 && scale <= 1.f);
			interp_pos = glm::mix(set->GetPos(src_idx, index0, clip_index).val, set->GetPos(src_idx, index1, clip_index).val, scale);
		}

		glm::quat interp_rot{};
		if (rot_idx == -1) {
			interp_rot = model->bones.at(src_idx).rot;
		}
		else if (rot_idx == set->GetChannel(clip_index, src_idx).num_rotations - 1)
			interp_rot = set->GetRot(src_idx, rot_idx, clip_index).val;
		else {
			int index0 = rot_idx;
			int index1 = rot_idx + 1;
			float t0 = set->GetRot(src_idx, index0, clip_index).time;
			float t1 = set->GetRot(src_idx, index1, clip_index).time;
			if (index0 == 0)t0 = 0.f;
			//float scale = MidLerp(clip.GetPos(i, index0).time, clip.GetPos(i, index1).time, curframe);
			//interp_pos = glm::mix(clip.GetPos(i, index0).val, clip.GetPos(i, index1).val, scale);
			float scale = MidLerp(t0, t1, curframe);
			assert(scale >= 0 && scale <= 1.f);
			interp_rot = glm::slerp(set->GetRot(src_idx, index0, clip_index).val, set->GetRot(src_idx, index1, clip_index).val, scale);
		}
		interp_rot = glm::normalize(interp_rot);

		pose.q[dest_idx] = interp_rot;
		pose.pos[dest_idx] = interp_pos;
	}
}

void util_set_to_bind_pose(Pose& pose, const Model* model)
{
	for (int i = 0; i < model->bones.size(); i++) {
		pose.pos[i] = model->bones.at(i).localtransform[3];
		pose.q[i] = model->bones.at(i).rot;
	}
}
