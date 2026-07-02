#include "AnimGraphTester.h"
#ifdef EDITOR_BUILD
#include "AnimGraphTesterEditorUI.h"
#endif
#include "Game/Components/MeshComponent.h"
#include "Game/Components/BillboardComponent.h"
#include "Render/Texture.h"
#include "Game/GameplayStatic.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/Runtime/RuntimeNodesNew2.h"
#include "Animation/SkeletonData.h"
#include "GameEnginePublic.h"
#include "Framework/Util.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include <algorithm>
#include <cmath>
#include "../../Debug.h"
static const StringName default_slot = StringName("MySlot");
static const StringName default_slot_2 = StringName("MySlot2");

static const uint32_t TERRAIN_MASK =
    (1u << (int)PL::Default) | (1u << (int)PL::StaticObject);

// Shortest-arc rotation taking unit vector `from` onto unit vector `to`. Robust at the
// antiparallel (180 deg) singularity, which glm::rotation handles poorly.
static glm::quat rotation_between(glm::vec3 from, glm::vec3 to) {
    from = glm::normalize(from);
    to   = glm::normalize(to);
    const float d = glm::clamp(glm::dot(from, to), -1.f, 1.f);
    if (d > 0.999999f)
        return glm::quat(1.f, 0.f, 0.f, 0.f);
    if (d < -0.999999f) {
        // Pick any axis orthogonal to `from`.
        glm::vec3 axis = glm::cross(glm::vec3(1.f, 0.f, 0.f), from);
        if (glm::length(axis) < 1e-4f)
            axis = glm::cross(glm::vec3(0.f, 1.f, 0.f), from);
        return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
    }
    const glm::vec3 axis = glm::cross(from, to);
    return glm::normalize(glm::angleAxis(std::acos(d), glm::normalize(axis)));
}

// -----------------------------------------------------------------------
AnimGraphTester::AnimGraphTester() {
    set_call_init_in_editor(true);
}

// -----------------------------------------------------------------------
void AnimGraphTester::editor_start() {
}

#ifdef EDITOR_BUILD
std::unique_ptr<IComponentEditorUi> AnimGraphTester::create_editor_ui() {
    return std::make_unique<AnimGraphTesterEditorUi>(this);
}

// Draws a sphere at each active agIk2Bone's resolved pole/joint-target position, so the
// bend direction can be tuned visually. pole_vis is filled by rebuild_graph(); no work
// happens here for modes without a pole configured (e.g. BasicIK).
void AnimGraphTester::editor_on_draw_gizmos_selected() {
    if (!mesh || pole_vis.empty())
        return;
    AnimatorObject* anim = mesh->get_animator();
    if (!anim)
        return;

    const auto& bonemats = anim->get_global_bonemats(); // mesh space
    const glm::mat4 ws_transform = get_owner()->get_ws_transform();
    const Color32 pole_color(255, 255, 0, 255);

    for (const PoleTargetVis& p : pole_vis) {
        glm::vec3 pole_ms = p.pole;
        if (p.in_bone_space) {
            const int idx = mesh->get_index_of_bone(p.pole_bone);
            if (idx < 0 || idx >= (int)bonemats.size())
                continue;
            pole_ms = glm::vec3(bonemats[idx] * glm::vec4(p.pole, 1.f));
        }
        const glm::vec3 pole_ws = glm::vec3(ws_transform * glm::vec4(pole_ms, 1.f));
        Debug::add_sphere(pole_ws, 0.04f, pole_color, 0.f, false);
    }
}
#endif

// -----------------------------------------------------------------------
void AnimGraphTester::start() {
    set_ticking(true);

    mesh = get_owner()->get_component<MeshComponent>();
    if (!mesh)
        mesh = get_owner()->create_component<MeshComponent>();

    if (model && !model->did_load_fail())
        mesh->set_model(model.get());
    else
        mesh->set_model_str("animman/models/animman.cmdl");

    // Spawn the moveable target entity (billboard, transient)
    Entity* tgt = eng->get_level()->spawn_entity();
    tgt->set_no_serialize();
    auto* bb = tgt->create_component<BillboardComponent>();
    bb->set_texture(Texture::load("eng/icon/_nearest/blue_poi.png"));
    tgt->set_ws_position(get_owner()->get_ws_position() + glm::vec3(0.f, 1.5f, 2.f));
    target_entity = tgt->get_self_ptr();

    rebuild_graph();
    rebuild_prop();
    last_mode = mode;
}

// -----------------------------------------------------------------------
void AnimGraphTester::stop() {
    if (Entity* tgt = target_entity.get())
        tgt->destroy();
    if (Entity* p = prop_entity.get())
        p->destroy();
}

// -----------------------------------------------------------------------
void AnimGraphTester::editor_on_change_property() {
    if (mesh) {
        // Sync model asset change to the MeshComponent
        if (model && !model->did_load_fail())
            mesh->set_model(model.get());
        rebuild_graph();
        rebuild_prop();
    }
}

// -----------------------------------------------------------------------
// Spawn (or respawn) a transient child mesh parented to prop_bone on the character.
// No-op cleanup happens unconditionally so editing the field swaps/clears the prop.
void AnimGraphTester::rebuild_prop() {
	auto local_transform = glm::mat4(1.f);
	if (Entity* old = prop_entity.get()) {
		local_transform = old->get_ls_transform();
        old->destroy();
	} else {
		local_transform = glm::mat4_cast(glm::quat(glm::vec3(glm::radians(90.f), 0.f, 0.f)));
    }
    prop_entity = EntityPtr();

    if (!prop || prop->did_load_fail())
        return;

    Entity* e = eng->get_level()->spawn_entity();
    e->set_no_serialize();
    auto* pm = e->create_component<MeshComponent>();
    pm->set_model(prop.get());
    e->parent_to(get_owner());
    e->set_parent_bone(StringName(prop_bone.c_str()));
	e->set_ls_transform(local_transform);
    prop_entity = e->get_self_ptr();
}

// -----------------------------------------------------------------------
void AnimGraphTester::rebuild_graph() {
    pole_vis.clear();
    if (!mesh || !mesh->get_model()) return;
    const Model* m = mesh->get_model();

    mesh->release_animator();

    agBuilder b;

    // Allocate a looping clip node from an AssetPtr; no-op if asset not set
    auto make_clip = [&](const AssetPtr<AnimationSeqAsset>& asset) -> agClipNode* {
        auto* n = b.alloc<agClipNode>();
        if (asset && !asset->did_load_fail())
            n->set_clip(asset.get());
        n->looping = true;
        return n;
    };
	auto make_eval_clip = [&](const AssetPtr<AnimationSeqAsset>& asset) -> agEvaluateClip* {
		auto* n = b.alloc<agEvaluateClip>();
		if (asset && !asset->did_load_fail())
			n->set_clip(asset.get());
		return n;
	};

    switch (mode) {

    case AnimGraphTestMode::BasicIK: {
        agClipNode* idle = make_clip(clip0);

        agIk2Bone* ik = b.alloc<agIk2Bone>();
        ik->input     = idle;
        ik->bone_name = StringName(bone_ik_end.c_str());  // hand is the end effector; solver bends forearm+upper arm
        ik->target    = StringName("vHandTarget");
		ik->alpha = generic_alpha;
		ik->allow_stretching = max_stretch > 1.f;
		ik->max_stretch_scale = max_stretch;
		ik->start_stretch_ratio = start_stretch_ratio;

        b.set_root(ik);
        break;
    }

    case AnimGraphTestMode::LookAt: {
        agClipNode* idle = make_clip(clip0);

        agModifyBone* mb = b.alloc<agModifyBone>();
        mb->input       = idle;
		mb->boneName = StringName("head");
        // Absolute mesh-space orientation (authoritative look-at). update() computes the full
        // desired head rotation each frame, so an additive delta here would double-apply / fight
        // last frame's result. The value is a quaternion variable, not euler.
        mb->rotation    = ModifyBoneType::Meshspace;
        mb->rotationVal = StringName("vHeadRot");
        mb->alpha       = generic_alpha;

        b.set_root(mb);
        break;
    }

    case AnimGraphTestMode::GunGripIK: {
        // 1. Base pose clip.


        agEvaluateClip* eval_clip = make_eval_clip(clip0);
		eval_clip->frame = gungrip_frame_eval;
		agBaseNode* pose = eval_clip;

        // 2-4. Layer an additive secondary motion (clip1) on top of the base. The additive
        //      is built at runtime as (clip1 - clip1's first frame), with the IK arm chain
        //      masked out so the add leaves those bones untouched. Only when clip1 is set.
        if (clip1 && !clip1->did_load_fail()) {
            agClipNode*     motion = make_clip(clip1);

            agAddNode* add = b.alloc<agAddNode>();
			add->input0 = pose;
			add->input1 = motion;
            add->alpha  = 1.f;
            pose = add;
        }

        auto make_ik_node = [&](StringName hand, StringName ik_grip, StringName polebone) -> agBaseNode* {
            // 5. IK the right hand to the gun grip on the left-hand bone (IK bones untouched above).
            agIk2Bone* ik = b.alloc<agIk2Bone>();
            ik->input              = pose;
			ik->bone_name = StringName(hand);
			ik->other_bone = StringName(ik_grip);
            ik->target             = glm::vec3(0.f, 0.f, 0.0f);
			ik->alpha = generic_alpha;
            ik->ik_in_bone_space   = true;
            ik->take_rotation_of_other = true;
			ik->allow_stretching = max_stretch > 1.f;
			ik->max_stretch_scale = max_stretch;
			ik->start_stretch_ratio = start_stretch_ratio;

            if (use_pole_bone_for_ik) {
				ik->pole_bone = polebone;
				ik->pole_in_bone_space = true;
				ik->pole = look_forward_axis;
				pole_vis.push_back({ ik->pole_bone, look_forward_axis, ik->pole_in_bone_space });
            }

            return ik;
		};
		pose = make_ik_node("hand_l", "ik_hand_l", "lowerarm_l");
		pose = make_ik_node("hand_r", "ik_hand_r", "lowerarm_r");


        b.set_root(pose);
        break;
    }

    case AnimGraphTestMode::FeetIK: {
        agBaseNode* node = make_clip(clip1);

        // Two-bone IK each foot directly to an ABSOLUTE mesh-space target supplied by
        // update() (vFootTarget*). The target is the foot's animated X/Z with Y regrounded
        // onto the surface, so there is no relative offset to feed back on. A modify-bone
        // AFTER the IK tilts the planted foot to the surface normal (vFootRot*).
		auto ik_foot = [&](agBaseNode* input, const std::string& bone, const char* tgtVar, const std::string& polebone) -> agIk2Bone* {
            agIk2Bone* ik = b.alloc<agIk2Bone>();
            ik->input            = input;
            ik->bone_name        = StringName(bone.c_str());
            ik->target           = StringName(tgtVar);   // absolute, in mesh space
            ik->ik_in_bone_space = false;
            ik->alpha            = StringName("flFootIkAlpha");
			ik->allow_stretching = max_stretch > 1.f;
			ik->max_stretch_scale = max_stretch;
			ik->start_stretch_ratio = start_stretch_ratio;
            if (use_pole_bone_for_ik) {
				ik->pole_in_bone_space = true;
				ik->pole_bone = StringName(polebone.c_str());
				ik->pole = look_forward_axis;
				pole_vis.push_back({ ik->pole_bone, look_forward_axis, ik->pole_in_bone_space });
            }

            return ik;
        };
        auto tilt_foot = [&](agBaseNode* input, const std::string& bone, const char* rotVar) -> agModifyBone* {
            agModifyBone* mb = b.alloc<agModifyBone>();
            mb->input       = input;
            mb->boneName    = StringName(bone.c_str());
            // Add the surface-normal tilt on top of the (now IK'd) foot's mesh-space rotation.
            mb->rotation    = ModifyBoneType::MeshspaceAdd;
            mb->rotationVal = StringName(rotVar);
            mb->alpha       = 1.f;
            return mb;
        };

        // Pelvis drop FIRST (so the feet IK re-plant relative to the lowered hips and the
        // reaching-down leg doesn't over-extend), then per-foot IK, then surface tilt.
        {
            agModifyBone* pelvis = b.alloc<agModifyBone>();
            pelvis->input          = node;
			pelvis->boneName = StringName("pelvis");
            pelvis->translation    = ModifyBoneType::MeshspaceAdd;
            pelvis->translationVal = StringName("vPelvisOffset");
            pelvis->alpha          = 1.f;
            node = pelvis;
        }

        node = ik_foot(node, "foot_l", "vFootTargetL", "calf_l");
		node = ik_foot(node, "foot_r", "vFootTargetR", "calf_r");
		node = tilt_foot(node, "foot_l", "vFootRotL");
		node = tilt_foot(node, "foot_r", "vFootRotR");

        b.set_root(node);
        break;
    }

    case AnimGraphTestMode::BlendMasked: {
        agClipNode* lower = make_clip(clip0);
        agClipNode* upper = make_clip(clip1);

        agBlendMasked* masked = b.alloc<agBlendMasked>();
        masked->init_mask_for_model(m, 0.f);
        masked->set_all_children_weights(m, bone_upper_blend, 1.f);
        masked->meshspace_blend = true;
        masked->input0 = lower;
        masked->input1 = upper;
        masked->alpha  = StringName("flBlendAlpha");

        b.set_root(masked);
        break;
    }

    case AnimGraphTestMode::CopyBone: {
        agClipNode* base = make_clip(clip0);

        agCopyBone* copy = b.alloc<agCopyBone>();
        copy->input           = base;
        copy->sourceBone      = StringName(bone_copy_src.c_str());
        copy->targetBone      = StringName(bone_copy_dst.c_str());
        copy->copyRotation    = true;
        copy->copyTranslation = false;
        copy->alpha           = 1.f;

        b.set_root(copy);
        break;
    }

    case AnimGraphTestMode::SlotPlaying: {
        b.add_slot_name(default_slot);

        agClipNode* idle = make_clip(clip0);

        agSlotPlayer* slot = b.alloc<agSlotPlayer>();
		slot->slotName = default_slot;
        slot->input    = idle;

        b.set_root(slot);
        slot_timer = 0.f;
        break;
    }

    case AnimGraphTestMode::Additive: {
        agClipNode* base    = make_clip(clip0);
        agClipNode* addClip = make_clip(clip1);

        agAddNode* addNode = b.alloc<agAddNode>();
        addNode->input0 = base;
        addNode->input1 = addClip;
        addNode->alpha  = StringName("flAddAlpha");

        b.set_root(addNode);
        break;
    }

    case AnimGraphTestMode::BlendByInt: {
        agClipNode* s0 = make_clip(clip0);
        agClipNode* s1 = make_clip(clip1);
        agClipNode* s2 = make_clip(clip2);

        agBlendByInt* bbi = b.alloc<agBlendByInt>();
        bbi->integer           = StringName("iState");
		bbi->easing = transition_easing;
        bbi->blending_duration = transition_time;
        bbi->inputs.push_back(s0);
        bbi->inputs.push_back(s1);
        bbi->inputs.push_back(s2);

        b.set_root(bbi);
        blend_state = 0;
        cycle_timer = 0.f;
        break;
    }
	case AnimGraphTestMode::DurationEventTest: {
		auto* clip = make_clip(clip0);
		b.set_root(clip);
		break;
	}

    case AnimGraphTestMode::BlendSpace2D: {
        agBlendSpace2D* bs = b.alloc<agBlendSpace2D>();
        if (clip0 && !clip0->did_load_fail()) bs->add_sample(clip0.get(), -1.f, -1.f);
        if (clip1 && !clip1->did_load_fail()) bs->add_sample(clip1.get(),  1.f, -1.f);
        if (clip2 && !clip2->did_load_fail()) bs->add_sample(clip2.get(), -1.f,  1.f);
        if (clip3 && !clip3->did_load_fail()) bs->add_sample(clip3.get(),  1.f,  1.f);
        bs->set_x_var("flBsX");
        bs->set_y_var("flBsY");
        bs->set_looping(true);
		bs->set_weight_speed(bs_input_smooth > 0.0001, bs_input_smooth);
		bs->set_x_smoothing(bs_smooth_time > 0.0001, bs_smooth_time, Easing::Linear);
		bs->set_y_smoothing(bs_smooth_time > 0.0001, bs_smooth_time, Easing::Linear);


        b.set_root(bs);
        break;
    }

    } // switch

    mesh->create_animator(&b);

    // Seed variables to safe defaults. The graph can evaluate before update() first runs,
    // and agIk2Bone reads its vec3 target unconditionally (get_vec3_var THROWS on a missing
    // variable, unlike floats which fall back to 0). Initializing here prevents that crash.
    if (AnimatorObject* a = mesh->get_animator()) {
        a->set_float_variable(StringName("flFootIkAlpha"), 0.f);
        a->set_vec3_variable(StringName("vFootTargetL"), glm::vec3(0.f));
        a->set_vec3_variable(StringName("vFootTargetR"), glm::vec3(0.f));
        a->set_vec3_variable(StringName("vFootRotL"),    glm::vec3(0.f));
        a->set_vec3_variable(StringName("vFootRotR"),    glm::vec3(0.f));
        a->set_vec3_variable(StringName("vPelvisOffset"),glm::vec3(0.f));
        a->set_quat_variable(StringName("vHeadRot"),     glm::quat(1.f, 0.f, 0.f, 0.f));
        a->set_float_variable(StringName("flBsX"), bs2d_x);
        a->set_float_variable(StringName("flBsY"), bs2d_y);
    }
}

// -----------------------------------------------------------------------
void AnimGraphTester::start_ik_dump() {
    ik_dump_remaining = 3.f;
    ik_dump_frame = 0;
    sys_print(Debug, "=== IK dump started (3 sec) ===\n");
}

// -----------------------------------------------------------------------
void AnimGraphTester::dump_ik_frame(int frame_num) {
    AnimatorObject* anim = mesh ? mesh->get_animator() : nullptr;
    if (!anim || !mesh->get_model()) return;

    const auto& bonemats = anim->get_global_bonemats();
    const MSkeleton* skel = mesh->get_model()->get_skel();
    if (!skel) return;

    // Chain must match what agIk2Bone actually solves: it takes the end-effector
    // bone and walks PARENTS up the skeleton. So b = parent(end), a = parent(parent(end)).
    // (Do NOT use bone_ik_upper/etc here — those are unrelated configs for other test
    //  modes and resolve to -1 on this model, which is why a/b showed "bone not found".)
    int end_idx   = mesh->get_index_of_bone(StringName(bone_ik_end.c_str()));
    int upper_idx = (end_idx   >= 0) ? skel->get_bone_parent(end_idx)   : -1; // b (mid/forearm)
    int root_idx  = (upper_idx >= 0) ? skel->get_bone_parent(upper_idx) : -1; // a (root/upper arm)

    int indices[3] = { end_idx, upper_idx, root_idx };
    const char* labels[3] = { "c(end/hand)", "b(mid/forearm)", "a(root/arm)" };

    // Target info
    Entity* tgt = target_entity.get();
    glm::vec3 target_ws = tgt ? tgt->get_ws_position() : get_owner()->get_ws_position();
    glm::mat4 ws_xf = get_owner()->get_ws_transform();
    glm::mat4 to_mesh = glm::inverse(ws_xf);
    glm::vec3 target_ms = glm::vec3(to_mesh * glm::vec4(target_ws, 1.f));

    sys_print(Debug, "--- IK frame %d ---\n", frame_num);
    sys_print(Debug, "  target  ws=(%.3f %.3f %.3f)  ms=(%.3f %.3f %.3f)\n",
        target_ws.x, target_ws.y, target_ws.z,
        target_ms.x, target_ms.y, target_ms.z);

    for (int i = 0; i < 3; i++) {
        int idx = indices[i];
        if (idx < 0 || idx >= (int)bonemats.size()) {
            sys_print(Debug, "  %s: bone not found (idx=%d)\n", labels[i], idx);
            continue;
        }

        // Mesh-space (global) transform from cached bonemats
        const glm::mat4& ms_mat = bonemats[idx];
        glm::vec3 ms_pos = glm::vec3(ms_mat[3]);
        glm::quat ms_rot = glm::quat_cast(glm::mat3(ms_mat));
        glm::vec3 ms_euler = glm::degrees(glm::eulerAngles(ms_rot));

        // World-space position
        glm::vec3 ws_pos = glm::vec3(ws_xf * glm::vec4(ms_pos, 1.f));

        // Local-space from skeleton pose (pos and rot stored per-bone)
        glm::vec3 loc_pos = glm::vec3(0.f);
        glm::vec3 loc_euler = glm::vec3(0.f);
        // Local data lives in the pose, which we can reconstruct from parent inverse
        int parent_idx = skel->get_bone_parent(idx);
        if (parent_idx >= 0 && parent_idx < (int)bonemats.size()) {
            glm::mat4 parent_inv = glm::inverse(bonemats[parent_idx]);
            glm::mat4 local_mat = parent_inv * ms_mat;
            loc_pos = glm::vec3(local_mat[3]);
            glm::quat loc_rot = glm::quat_cast(glm::mat3(local_mat));
            loc_euler = glm::degrees(glm::eulerAngles(loc_rot));
        } else {
            loc_pos = ms_pos;
            glm::quat loc_rot = ms_rot;
            loc_euler = glm::degrees(glm::eulerAngles(loc_rot));
        }

        // Distance from this bone to target (mesh space)
        float dist_to_target = glm::length(ms_pos - target_ms);

        sys_print(Debug, "  %s (bone %d):\n", labels[i], idx);
        sys_print(Debug, "    local  pos=(%.3f %.3f %.3f)  euler=(%.1f %.1f %.1f)\n",
            loc_pos.x, loc_pos.y, loc_pos.z, loc_euler.x, loc_euler.y, loc_euler.z);
        sys_print(Debug, "    mesh   pos=(%.3f %.3f %.3f)  euler=(%.1f %.1f %.1f)\n",
            ms_pos.x, ms_pos.y, ms_pos.z, ms_euler.x, ms_euler.y, ms_euler.z);
        sys_print(Debug, "    world  pos=(%.3f %.3f %.3f)  dist_to_target=%.4f\n",
            ws_pos.x, ws_pos.y, ws_pos.z, dist_to_target);
    }

    // End effector error: how far is c (hand) from target in mesh space?
    if (end_idx >= 0 && end_idx < (int)bonemats.size()) {
        glm::vec3 c_ms = glm::vec3(bonemats[end_idx][3]);
        float err = glm::length(c_ms - target_ms);
        sys_print(Debug, "  IK ERROR (hand ms - target ms) = %.4f m\n", err);
    }
}

// -----------------------------------------------------------------------
void AnimGraphTester::update() {
    if (mode != last_mode || (mesh && mesh->get_model() && !mesh->get_animator())) {
        rebuild_graph();
        last_mode = mode;
    }

    AnimatorObject* anim = mesh ? mesh->get_animator() : nullptr;
    if (!anim) return;

    const float dt = eng->get_dt();
    cycle_timer += dt;

    Entity* tgt = target_entity.get();
    glm::vec3 target_ws = tgt ? tgt->get_ws_position() : get_owner()->get_ws_position();

    glm::mat4 ws_transform = get_owner()->get_ws_transform();
    glm::mat4 to_mesh = glm::inverse(ws_transform);

    auto world_to_mesh = [&](glm::vec3 wp) {
        return glm::vec3(to_mesh * glm::vec4(wp, 1.f));
    };
    auto mesh_to_world = [&](glm::vec3 mp) {
        return glm::vec3(ws_transform * glm::vec4(mp, 1.f));
    };
    switch (mode) {

    case AnimGraphTestMode::BasicIK:
        anim->set_vec3_variable(StringName("vHandTarget"), world_to_mesh(target_ws));
        break;

    case AnimGraphTestMode::LookAt: {
        const MSkeleton* skel = mesh->get_model() ? mesh->get_model()->get_skel() : nullptr;
        const auto& bonemats = anim->get_global_bonemats();
		int headIdx = mesh->get_index_of_bone(StringName("head"));
        if (skel && headIdx >= 0 && headIdx < (int)bonemats.size()) {
            // Reference frame MUST be one the modify-bone never overwrites, otherwise reading
            // the live bonemats back in (which already contain last frame's look-at) forms a
            // feedback loop whose equilibrium ignores look_forward_axis entirely. So aim from
            // the BIND pose (constant) -- only the head POSITION is taken from the live pose so
            // the target direction is correct.
            const glm::vec3   head_pos  = glm::vec3(bonemats[headIdx][3]);
            const glm::mat4x3& bind      = skel->get_all_bones()[headIdx].posematrix; // bone->mesh
            const glm::quat   rest_rot  = glm::normalize(glm::quat_cast(glm::mat3(bind[0], bind[1], bind[2])));

            const glm::vec3 to_target = world_to_mesh(target_ws) - head_pos;
            if (glm::length(to_target) > 1e-5f) {
                const glm::vec3 desired_dir = glm::normalize(to_target);
                // Rest-pose mesh-space facing of the head's local "face" axis, then a
                // minimal-twist (shortest-arc) aim onto the target. Fully determined, no
                // feedback. Delivered as a quaternion -- no euler round-trip.
                const glm::vec3 rest_forward = rest_rot * glm::normalize(look_forward_axis);
                const glm::quat aim = rotation_between(rest_forward, desired_dir);
                anim->set_quat_variable(StringName("vHeadRot"), glm::normalize(aim * rest_rot));
            }
        }
        break;
    }

    case AnimGraphTestMode::GunGripIK:
        break;

    case AnimGraphTestMode::FeetIK: {
        const auto& bonemats = anim->get_global_bonemats();
        const glm::quat R = glm::normalize(glm::quat_cast(glm::mat3(ws_transform))); // mesh -> world
        const float max_tilt = glm::radians(foot_max_tilt_deg);
        // Reference plane the animation plants its feet on (the character's base/origin).
        // Foot IK adds the terrain delta relative to THIS, so the animated foot lift is kept.
        const float root_y = get_owner()->get_ws_position().y;

        // Per foot: trace straight down from the IK BONE (the animation-driven reference
        // that the IK below never modifies, so reading it never feeds back), then hand the
        // real foot an ABSOLUTE mesh-space target = (IK bone X/Z, surface Y + height off).
        // Because the target is absolute and the trace is vertical (ground Y is independent
        // of the foot's current Y), there is no feedback loop -- only a 1-frame latency.

        glm::vec3 foot_l_pos = ws_transform[3];
		glm::vec3 foot_r_pos = ws_transform[3];

        auto ground_foot = [&](const std::string& ik_bone_name, const char* tgtVar, const char* rotVar, float& out_ground_delta, glm::vec3& out_foot_pos) -> bool {
            out_ground_delta = 0.f;   // flat / no-hit contributes nothing to the pelvis drop
            int idx = mesh->get_index_of_bone(StringName(ik_bone_name.c_str()));
            if (idx < 0 || idx >= (int)bonemats.size()) return false;

            auto foot_mesh = glm::vec3(bonemats[idx][3]);
			glm::vec3 foot_world = glm::vec3(ws_transform * glm::vec4(foot_mesh, 1.f));
            HitResult hit = GameplayStatic::cast_ray(
                foot_world + glm::vec3(0.f,  foot_trace_dist, 0.f),
                foot_world + glm::vec3(0.f, -foot_trace_dist, 0.f),
                TERRAIN_MASK, nullptr);

            // Target defaults to the current foot pos (IK is a no-op) when nothing is hit.
            glm::vec3 target_world = foot_world;
            glm::vec3 rot_euler(0.f);
            if (hit.hit) {
				Debug::add_sphere(glm::vec3(foot_world.x, hit.pos.y, foot_world.z), 0.1, COLOR_PINK, 0.f);
                // ADD the terrain delta to the animated foot height (don't pin to the ground),
                // so a lifted/swinging foot stays lifted and only the planted foot lands.
                const float terrain_delta = (hit.pos.y + foot_height_off) - root_y;
                target_world.y = foot_world.y + terrain_delta;
                // How far below the base plane this foot's ground is (used for the pelvis drop).
                out_ground_delta = hit.pos.y - root_y;
                if (foot_align_rot) {
                    // Shortest-arc rotation from world-up to surface normal, clamped, then
                    // expressed in mesh space (conjugation by the mesh->world rotation).
                    glm::vec3 n = glm::normalize(hit.normal);
                    glm::vec3 axis = glm::cross(glm::vec3(0.f, 1.f, 0.f), n);
                    float axis_len = glm::length(axis);
                    if (axis_len > 1e-4f) {
                        float ang = std::min(std::acos(glm::clamp(n.y, -1.f, 1.f)), max_tilt);
                        glm::quat q_mesh = glm::inverse(R) * glm::angleAxis(ang, axis / axis_len) * R;
                        rot_euler = glm::eulerAngles(q_mesh);
                    }
                }
            }

            // Absolute mesh-space target: keep the animated foot's X/Z, ground only Y.
            glm::vec3 in_meshspace = world_to_mesh(target_world);
			glm::vec3 foot_target  = glm::vec3(foot_mesh.x, in_meshspace.y, foot_mesh.z);

            anim->set_vec3_variable(StringName(tgtVar), foot_target);
            anim->set_vec3_variable(StringName(rotVar), rot_euler);

            out_foot_pos = mesh_to_world(foot_target);

            return true;
        };

        // Only enable IK once both targets resolve; otherwise an unset/zero target would
        // yank the foot toward the mesh origin.
        float deltaL = 0.f, deltaR = 0.f;
		bool okL = ground_foot("ik_foot_l", "vFootTargetL", "vFootRotL", deltaL, foot_l_pos);
		bool okR = ground_foot("ik_foot_r", "vFootTargetR", "vFootRotR", deltaR, foot_r_pos);
        anim->set_float_variable(StringName("flFootIkAlpha"), (okL && okR) ? 1.f : 0.f);

        // Pelvis drop = how far the LOWEST-ground foot must reach below the base plane
        // (only ever down, never up). Interpolated so terrain changes don't pop the hips.
        const float pelvis_target = (okL && okR) ? std::min({ deltaL, deltaR, 0.f }) : 0.f;
        const float t = glm::clamp(pelvis_interp_speed * dt, 0.f, 1.f);
        pelvis_offset += (pelvis_target - pelvis_offset) * t;
        anim->set_vec3_variable(StringName("vPelvisOffset"), glm::vec3(0.f, pelvis_offset, 0.f));

        if (footstep_particle.get()) {
			for (auto& e : anim->sampled_events) {
				if (e.event->name == "foot_r") {
					// spawn particle system at footstep
					GameplayStatic::spawn_particle_effect(footstep_particle.get(), foot_r_pos);
				}
				else if (e.event->name == "foot_l") {
					// spawn particle system at footstep
					GameplayStatic::spawn_particle_effect(footstep_particle.get(), foot_l_pos);
				}
			}
		}
        break;
    }

    case AnimGraphTestMode::BlendMasked: {
        float alpha = 0.5f + 0.5f * sinf(cycle_timer * glm::pi<float>() * 0.5f);
        anim->set_float_variable(StringName("flBlendAlpha"), alpha);
        break;
    }

    case AnimGraphTestMode::CopyBone:
        break;

    case AnimGraphTestMode::SlotPlaying:
        slot_timer += dt;
        if (slot_timer >= 3.f) {
            slot_timer = 0.f;
            if (slot_clip && !slot_clip->did_load_fail())
				anim->play_animation(default_slot,slot_clip.get(), 1.f, 0.f);
        }
        break;

    case AnimGraphTestMode::Additive: {
        float alpha = 0.5f + 0.5f * sinf(cycle_timer * glm::pi<float>());
        anim->set_float_variable(StringName("flAddAlpha"), alpha);
        break;
    }

    case AnimGraphTestMode::BlendByInt:
        if (cycle_timer >= 2.f) {
            cycle_timer = 0.f;
            blend_state = (blend_state + 1) % 3;
        }
        anim->set_int_variable(StringName("iState"), blend_state);
        break;
	case AnimGraphTestMode::DurationEventTest: {

        bool in_event = false;
        for (auto& anim_event : anim->sampled_events) {
			if (anim_event.event->name == "roll") {
			
                if (anim_event.trigger == AnimEventTrigger::Entered) {
					if (footstep_particle.get()) {
						GameplayStatic::spawn_particle_effect(footstep_particle.get(), get_ws_position());
					}
					in_event = true;
                }
                else if (anim_event.trigger == AnimEventTrigger::Active) {
					in_event = true;
                }
            }
        }
		if (in_event) {
			mesh->set_material_override(matoverride.get());
        }
        else {
			mesh->set_material_override(nullptr);
        }

        break;
    }

    case AnimGraphTestMode::BlendSpace2D: {
        // Auto-sweep a circle over the 4-corner grid unless the editor preview UI has taken
        // manual control (dragging the marker there sets bs2d_manual and bs2d_x/y directly).
        if (!bs2d_manual) {
            bs2d_x = cosf(cycle_timer * 0.5f);
            bs2d_y = sinf(cycle_timer * 0.5f);
        }
        anim->set_float_variable(StringName("flBsX"), bs2d_x);
        anim->set_float_variable(StringName("flBsY"), bs2d_y);
        break;
    }

    } // switch

    if (ik_dump_remaining > 0.f) {
        dump_ik_frame(ik_dump_frame++);
        ik_dump_remaining -= dt;
        if (ik_dump_remaining <= 0.f)
            sys_print(Debug, "=== IK dump ended (%d frames) ===\n", ik_dump_frame);
    }
}
