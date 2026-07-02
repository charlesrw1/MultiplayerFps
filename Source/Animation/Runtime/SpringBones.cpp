#include "SpringBones.h"
#include "Framework/Util.h"
#include "Animation/AnimationTypes.h"
#include "Animation/SkeletonData.h"
#include <algorithm>

// Called with data-driven/editor-authored bone names, which can transiently be invalid
// (asset not loaded yet, typo mid-edit, skeleton root picked) -- log and no-op rather than
// ASSERT-crashing the editor.
void SpringBones::add_spring_bone(const MSkeleton* skel, StringName boneName, const SpringBoneParams& params) {
	if (!skel)
		return;
	const int boneIndex = skel->get_bone_index(boneName);
	if (boneIndex == -1) {
		sys_print(Error, "SpringBones::add_spring_bone: bone '%s' not found on skeleton\n", boneName.get_c_str());
		return;
	}
	if (skel->get_bone_parent(boneIndex) == -1) {
		sys_print(Error, "SpringBones::add_spring_bone: bone '%s' is the skeleton root, can't be a spring bone\n",
				  boneName.get_c_str());
		return;
	}

	SpringBoneDef def;
	def.boneName = boneName;
	def.boneIndex = boneIndex;
	def.parentIndex = skel->get_bone_parent(boneIndex);
	def.params = params;

	defs.push_back(def);
	states.emplace_back();
	needsSort = true;
}

// Rotation that takes unit vector `from` to unit vector `to`. Avoids pulling in
// glm/gtx (experimental) just for glm::rotation().
static glm::quat quat_from_to(const glm::vec3& from, const glm::vec3& to) {
	const float d = glm::dot(from, to);
	if (d > 0.9999f)
		return glm::quat(1.f, 0.f, 0.f, 0.f);
	if (d < -0.9999f) {
		// 180 degrees apart, pick any orthogonal axis
		glm::vec3 axis = glm::cross(glm::vec3(1.f, 0.f, 0.f), from);
		if (glm::dot(axis, axis) < 1e-8f)
			axis = glm::cross(glm::vec3(0.f, 1.f, 0.f), from);
		return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
	}
	const glm::vec3 axis = glm::cross(from, to);
	const float s = glm::sqrt((1.f + d) * 2.f);
	return glm::normalize(glm::quat(s * 0.5f, axis / s));
}

static glm::mat4 compose_local(const Pose& pose, int bone) {
	glm::mat4 m = glm::mat4_cast(pose.q[bone]);
	m[3] = glm::vec4(pose.pos[bone], 1.0f);
	return glm::scale(m, glm::vec3(pose.scale[bone]));
}

// Builds a stable (yaw, pitch) frame perpendicular to `along`. `along` generally changes
// smoothly frame-to-frame (it's an animated bone direction), so this construction -- while
// redone from scratch every update rather than parallel-transported -- varies smoothly too.
static void build_bend_axes(const glm::vec3& along, glm::vec3& outYawAxis, glm::vec3& outPitchAxis) {
	glm::vec3 reference = (glm::abs(along.y) < 0.99f) ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(0.f, 0.f, 1.f);
	outPitchAxis = glm::normalize(glm::cross(reference, along));
	outYawAxis = glm::cross(along, outPitchAxis);
}

void SpringBones::update(float dt, const Pose& pose, std::vector<glm::mat4>& cached_bonemats, const MSkeleton* skel,
						  const glm::mat4& ownerWorldTransform) {
	if (defs.empty())
		return;

	if (needsSort) {
		// index-sort defs/states together so parents (lower bone index) update before children
		std::vector<int> order(defs.size());
		for (int i = 0; i < (int)order.size(); i++)
			order[i] = i;
		std::sort(order.begin(), order.end(), [this](int a, int b) { return defs[a].boneIndex < defs[b].boneIndex; });

		std::vector<SpringBoneDef> newDefs(defs.size());
		std::vector<SpringBoneState> newStates(states.size());
		for (int i = 0; i < (int)order.size(); i++) {
			newDefs[i] = defs[order[i]];
			newStates[i] = states[order[i]];
		}
		defs = std::move(newDefs);
		states = std::move(newStates);
		needsSort = false;
	}

	const glm::mat4 meshToWorld = ownerWorldTransform;
	const glm::mat4 worldToMesh = glm::inverse(ownerWorldTransform);

	for (int i = 0; i < (int)defs.size(); i++) {
		const SpringBoneDef& def = defs[i];
		const SpringBoneParams& p = def.params;
		SpringBoneState& state = states[i];

		ASSERT(def.parentIndex >= 0 && "spring bones must not be the skeleton root");
		const glm::mat4& parentMat = cached_bonemats[def.parentIndex]; // mesh space, possibly already spring-simulated

		// "Rest" = where the bone would be if it rigidly followed its (possibly already
		// spring-simulated) parent with no independent motion of its own.
		const glm::mat4 restMat = parentMat * compose_local(pose, def.boneIndex);
		const glm::mat4 worldRestMat = meshToWorld * restMat;
		const glm::vec3 worldRestPos = glm::vec3(worldRestMat[3]);
		const glm::vec3 worldParentPos = glm::vec3(meshToWorld * parentMat[3]);

		const glm::vec3 toRest = worldRestPos - worldParentPos;
		const float restLen = glm::length(toRest);
		const glm::vec3 along = (restLen > 1e-5f) ? (toRest / restLen) : glm::vec3(0.f, 0.f, 1.f);

		if (!state.initialized) {
			state.currentPos = worldRestPos;
			state.velocity = glm::vec3(0.f);
			state.initialized = true;
		}

		glm::vec3 yawAxis, pitchAxis;
		build_bend_axes(along, yawAxis, pitchAxis);

		// Deflection of the actual (simulated) tip from the animated rest tip, decomposed
		// into the along/yaw/pitch frame -- each axis is an independent damped oscillator.
		const glm::vec3 dev = state.currentPos - worldRestPos;
		const float devYaw = glm::dot(dev, yawAxis);
		const float devPitch = glm::dot(dev, pitchAxis);
		const float devAlong = glm::dot(dev, along);

		const float velYaw = glm::dot(state.velocity, yawAxis);
		const float velPitch = glm::dot(state.velocity, pitchAxis);
		const float velAlong = glm::dot(state.velocity, along);

		const glm::vec3 gravityAccel = p.gravityDir * p.gravity;

		const float accYaw = -p.yaw_stiffness * devYaw - p.yaw_damping * velYaw + glm::dot(gravityAccel, yawAxis);
		const float accPitch =
			-p.pitch_stiffness * devPitch - p.pitch_damping * velPitch + glm::dot(gravityAccel, pitchAxis);
		const float accAlong = p.allow_length_flex
									? (-p.along_stiffness * devAlong - p.along_damping * velAlong +
									   glm::dot(gravityAccel, along))
									: 0.f;

		const float newVelYaw = velYaw + accYaw * dt;
		const float newVelPitch = velPitch + accPitch * dt;
		const float newVelAlong = p.allow_length_flex ? (velAlong + accAlong * dt) : 0.f;

		const float newDevYaw = devYaw + newVelYaw * dt;
		const float newDevPitch = devPitch + newVelPitch * dt;
		const float newDevAlong = p.allow_length_flex ? (devAlong + newVelAlong * dt) : 0.f;

		glm::vec3 newPos = worldRestPos + newDevYaw * yawAxis + newDevPitch * pitchAxis + newDevAlong * along;
		state.velocity = newVelYaw * yawAxis + newVelPitch * pitchAxis + newVelAlong * along;

		// Rigid-length bones: position-constrain back onto the sphere of radius restLen around
		// the parent every update, rather than letting the along-axis spring do it (it's inert).
		if (!p.allow_length_flex && restLen > 1e-5f) {
			glm::vec3 dir = newPos - worldParentPos;
			const float dirLen = glm::length(dir);
			if (dirLen > 1e-5f)
				newPos = worldParentPos + dir / dirLen * restLen;
		}

		state.currentPos = newPos;

		glm::vec3 curDir = newPos - worldParentPos;
		const float curDirLen = glm::length(curDir);

		glm::mat4 worldResultMat;
		if (curDirLen > 1e-5f && restLen > 1e-5f) {
			const glm::quat delta = quat_from_to(along, curDir / curDirLen);
			const glm::quat newRot = delta * glm::quat_cast(worldRestMat);

			worldResultMat = glm::mat4_cast(newRot);
			worldResultMat[3] = glm::vec4(newPos, 1.0f);
		} else {
			worldResultMat = worldRestMat;
		}

		cached_bonemats[def.boneIndex] = worldToMesh * worldResultMat;
	}
}
