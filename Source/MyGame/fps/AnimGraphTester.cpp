#include "AnimGraphTester.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/BillboardComponent.h"
#include "Game/GameplayStatic.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/Runtime/RuntimeNodesNew2.h"
#include "Animation/SkeletonData.h"
#include "GameEnginePublic.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"

static const uint32_t TERRAIN_MASK =
    (1u << (int)PL::Default) | (1u << (int)PL::StaticObject);

// -----------------------------------------------------------------------
AnimGraphTester::AnimGraphTester() {
    set_call_init_in_editor(true);
}

// -----------------------------------------------------------------------
std::string AnimGraphTester::safe_clip(const Model* m, const std::string& name) const {
    if (!m || !m->get_skel()) return name;
    const auto& clips = m->get_skel()->get_all_clips();
    if (clips.count(name)) return name;
    return clips.empty() ? name : clips.begin()->first;
}

// -----------------------------------------------------------------------
void AnimGraphTester::editor_start() {
    editor_set_model("animman.cmdl", false);
}

// -----------------------------------------------------------------------
void AnimGraphTester::start() {
    set_ticking(true);

    // Get or create the MeshComponent on this entity
    mesh = get_owner()->get_component<MeshComponent>();
    if (!mesh) {
        mesh = get_owner()->create_component<MeshComponent>();
        mesh->set_model_str("animman.cmdl");
    }

    // Spawn the moveable target entity (billboard, not serialized)
    Entity* tgt = eng->get_level()->spawn_entity();
    tgt->set_editor_transient(true);
    auto* bb = tgt->create_component<BillboardComponent>();
    (void)bb; // no special texture needed — default billboard is fine
    glm::vec3 start_pos = get_owner()->get_ws_position() + glm::vec3(0.f, 1.5f, 2.f);
    tgt->set_ws_position(start_pos);
    target_entity = tgt->get_self_ptr();

    rebuild_graph();
    last_mode = mode;
}

// -----------------------------------------------------------------------
void AnimGraphTester::stop() {
    // target entity cleans itself up with the level
}

// -----------------------------------------------------------------------
void AnimGraphTester::editor_on_change_property() {
    if (mesh) rebuild_graph();
}

// -----------------------------------------------------------------------
void AnimGraphTester::rebuild_graph() {
    if (!mesh || !mesh->get_model()) return;
    const Model* model = mesh->get_model();

    mesh->release_animator();

    agBuilder b;

    // ------------------------------------------------------------------
    // Helpers: allocate a looping clip node
    auto make_clip = [&](const std::string& name) -> agClipNode* {
        auto* n = b.alloc<agClipNode>();
        n->set_clip(model, safe_clip(model, name));
        n->looping = true;
        return n;
    };

    switch (mode) {

    // ------------------------------------------------------------------
    case AnimGraphTestMode::BasicIK: {
        // idle → IK right hand to "vHandTarget" (mesh-space vec3)
        agClipNode* idle = make_clip(clip0);

        agIk2Bone* ik = b.alloc<agIk2Bone>();
        ik->input       = idle;
        ik->bone_name   = StringName(bone_ik_upper.c_str());
        ik->other_bone  = StringName(bone_ik_end.c_str());
        ik->target      = StringName("vHandTarget");
        ik->alpha       = 1.f;

        b.set_root(ik);
        break;
    }

    // ------------------------------------------------------------------
    case AnimGraphTestMode::LookAt: {
        // idle → agModifyBone head (MeshspaceAdd rotation from "vHeadRot")
        agClipNode* idle = make_clip(clip0);

        agModifyBone* mb = b.alloc<agModifyBone>();
        mb->input       = idle;
        mb->boneName    = StringName(bone_head.c_str());
        mb->rotation    = ModifyBoneType::MeshspaceAdd;
        mb->rotationVal = StringName("vHeadRot");
        mb->alpha       = 1.f;

        b.set_root(mb);
        break;
    }

    // ------------------------------------------------------------------
    case AnimGraphTestMode::GunGripIK: {
        // IK right forearm/hand to offset from left hand bone
        // ik_in_bone_space=true means target is in other_bone's local space
        agClipNode* idle = make_clip(clip0);

        agIk2Bone* ik = b.alloc<agIk2Bone>();
        ik->input             = idle;
        ik->bone_name         = StringName(bone_ik_upper.c_str());
        ik->other_bone        = StringName(bone_grip_other.c_str());
        ik->target            = glm::vec3(0.f, 0.f, 0.2f); // fixed offset in other_bone space
        ik->alpha             = 1.f;
        ik->ik_in_bone_space  = true;
        ik->take_rotation_of_other = true;

        b.set_root(ik);
        break;
    }

    // ------------------------------------------------------------------
    case AnimGraphTestMode::FeetIK: {
        // walk → IK left foot → IK right foot (targets set by raycasts each frame)
        agClipNode* walk = make_clip(clip1);

        agIk2Bone* ikL = b.alloc<agIk2Bone>();
        ikL->input      = walk;
        ikL->bone_name  = StringName(bone_foot_l.c_str());
        ikL->target     = StringName("vFootTargetL");
        ikL->alpha      = StringName("flFootIkAlpha");

        agIk2Bone* ikR = b.alloc<agIk2Bone>();
        ikR->input      = ikL;
        ikR->bone_name  = StringName(bone_foot_r.c_str());
        ikR->target     = StringName("vFootTargetR");
        ikR->alpha      = StringName("flFootIkAlpha");

        b.set_root(ikR);
        break;
    }

    // ------------------------------------------------------------------
    case AnimGraphTestMode::BlendMasked: {
        // clip0 (lower body) + clip1 (upper body) masked at spine
        agClipNode* lower = make_clip(clip0);
        agClipNode* upper = make_clip(clip1);

        agBlendMasked* masked = b.alloc<agBlendMasked>();
        masked->init_mask_for_model(model, 0.f);
        masked->set_all_children_weights(model, bone_upper_blend, 1.f);
        masked->meshspace_blend = true;
        masked->input0 = lower;
        masked->input1 = upper;
        masked->alpha  = StringName("flBlendAlpha");

        b.set_root(masked);
        break;
    }

    // ------------------------------------------------------------------
    case AnimGraphTestMode::CopyBone: {
        // clip0 → copy spine rotation to neck/head
        agClipNode* base = make_clip(clip0);

        agCopyBone* copy = b.alloc<agCopyBone>();
        copy->input          = base;
        copy->sourceBone     = StringName(bone_copy_src.c_str());
        copy->targetBone     = StringName(bone_copy_dst.c_str());
        copy->copyRotation   = true;
        copy->copyTranslation = false;
        copy->alpha          = 1.f;

        b.set_root(copy);
        break;
    }

    // ------------------------------------------------------------------
    case AnimGraphTestMode::SlotPlaying: {
        // idle base with a slot player; clips are fired into "TestSlot" on a timer
        b.add_slot_name(StringName("TestSlot"));

        agClipNode* idle = make_clip(clip0);

        agSlotPlayer* slot = b.alloc<agSlotPlayer>();
        slot->slotName = StringName("TestSlot");
        slot->input    = idle;

        b.set_root(slot);
        slot_timer = 0.f;
        break;
    }

    // ------------------------------------------------------------------
    case AnimGraphTestMode::Additive: {
        // clip0 (base) + clip1 (additive) — alpha pulses 0→1→0
        agClipNode* base = make_clip(clip0);
        agClipNode* add  = make_clip(clip1);

        agAddNode* addNode = b.alloc<agAddNode>();
        addNode->input0 = base;
        addNode->input1 = add;
        addNode->alpha  = StringName("flAddAlpha");

        b.set_root(addNode);
        break;
    }

    // ------------------------------------------------------------------
    case AnimGraphTestMode::BlendByInt: {
        // three clips blended by cycling integer "iState"
        agClipNode* s0 = make_clip(clip0);
        agClipNode* s1 = make_clip(clip1);
        agClipNode* s2 = make_clip(clip2);

        agBlendByInt* bbi = b.alloc<agBlendByInt>();
        bbi->integer          = StringName("iState");
        bbi->easing           = Easing::CubicEaseOut;
        bbi->blending_duration = 0.4f;
        bbi->inputs.push_back(s0);
        bbi->inputs.push_back(s1);
        bbi->inputs.push_back(s2);

        b.set_root(bbi);
        blend_state = 0;
        cycle_timer = 0.f;
        break;
    }

    } // switch

    mesh->create_animator(&b);
}

// -----------------------------------------------------------------------
void AnimGraphTester::update() {
    // Hot-swap graph when mode property changes at runtime
    if (mode != last_mode) {
        rebuild_graph();
        last_mode = mode;
    }

    AnimatorObject* anim = mesh ? mesh->get_animator() : nullptr;
    if (!anim) return;

    const float dt = eng->get_dt();
    cycle_timer += dt;

    Entity* tgt = target_entity.get();
    glm::vec3 target_ws = tgt ? tgt->get_ws_position() : get_owner()->get_ws_position();

    // mesh-space inverse transform for converting world positions
    glm::mat4 ws_transform = get_owner()->get_ws_transform();
    glm::mat4 to_mesh = glm::inverse(ws_transform);

    auto world_to_mesh = [&](glm::vec3 wp) {
        return glm::vec3(to_mesh * glm::vec4(wp, 1.f));
    };

    switch (mode) {

    case AnimGraphTestMode::BasicIK: {
        anim->set_vec3_variable(StringName("vHandTarget"), world_to_mesh(target_ws));
        break;
    }

    case AnimGraphTestMode::LookAt: {
        // Compute delta mesh-space rotation that rotates the head toward target
        const auto& bonemats = anim->get_global_bonemats();
        int headIdx = mesh->get_index_of_bone(StringName(bone_head.c_str()));
        if (headIdx >= 0 && headIdx < (int)bonemats.size()) {
            glm::vec3 head_mesh_pos = glm::vec3(bonemats[headIdx][3]);
            glm::vec3 target_mesh   = world_to_mesh(target_ws);
            glm::vec3 dir = glm::normalize(target_mesh - head_mesh_pos);

            // Quaternion that points +Z forward at the target (mesh space)
            glm::quat desired = glm::quatLookAt(dir, glm::vec3(0.f, 1.f, 0.f));
            // Current head mesh-space orientation from last frame
            glm::quat current = glm::quat_cast(glm::mat3(bonemats[headIdx]));
            // Delta rotation so MeshspaceAdd: delta * current = desired
            glm::quat delta = desired * glm::inverse(current);
            anim->set_vec3_variable(StringName("vHeadRot"), glm::eulerAngles(delta));
        }
        break;
    }

    case AnimGraphTestMode::GunGripIK:
        // Graph is fully driven by the fixed bone-space offset — nothing to update
        break;

    case AnimGraphTestMode::FeetIK: {
        const auto& bonemats = anim->get_global_bonemats();
        anim->set_float_variable(StringName("flFootIkAlpha"), 1.f);

        auto ik_foot = [&](const std::string& bone_name, StringName var_name) {
            int idx = mesh->get_index_of_bone(StringName(bone_name.c_str()));
            if (idx < 0 || idx >= (int)bonemats.size()) return;
            glm::vec3 foot_mesh = glm::vec3(bonemats[idx][3]);
            glm::vec3 foot_world = glm::vec3(ws_transform * glm::vec4(foot_mesh, 1.f));
            glm::vec3 ray_start = foot_world + glm::vec3(0.f,  0.5f, 0.f);
            glm::vec3 ray_end   = foot_world + glm::vec3(0.f, -0.5f, 0.f);
            HitResult hit = GameplayStatic::cast_ray(ray_start, ray_end, TERRAIN_MASK, nullptr);
            glm::vec3 ik_world = hit.hit ? hit.pos : foot_world;
            anim->set_vec3_variable(var_name, world_to_mesh(ik_world));
        };

        ik_foot(bone_foot_l, StringName("vFootTargetL"));
        ik_foot(bone_foot_r, StringName("vFootTargetR"));
        break;
    }

    case AnimGraphTestMode::BlendMasked: {
        // Blend alpha cycles 0 → 1 → 0 over 4 seconds
        float alpha = 0.5f + 0.5f * sinf(cycle_timer * glm::pi<float>() * 0.5f);
        anim->set_float_variable(StringName("flBlendAlpha"), alpha);
        break;
    }

    case AnimGraphTestMode::CopyBone:
        // Driven by the animation; nothing extra to update
        break;

    case AnimGraphTestMode::SlotPlaying: {
        // Fire a one-shot clip into the slot every 3 seconds
        slot_timer += dt;
        if (slot_timer >= 3.f) {
            slot_timer = 0.f;
            if (slot_clip && !slot_clip->did_load_fail()) {
                anim->play_animation(slot_clip.get(), 1.f, 0.f);
            }
        }
        break;
    }

    case AnimGraphTestMode::Additive: {
        // Additive alpha pulses 0 → 1 over 2 seconds
        float alpha = 0.5f + 0.5f * sinf(cycle_timer * glm::pi<float>());
        anim->set_float_variable(StringName("flAddAlpha"), alpha);
        break;
    }

    case AnimGraphTestMode::BlendByInt: {
        // Cycle through states 0, 1, 2 every 2 seconds
        if (cycle_timer >= 2.f) {
            cycle_timer = 0.f;
            blend_state = (blend_state + 1) % 3;
            anim->set_int_variable(StringName("iState"), blend_state);
        }
        // Set on every frame in case animator was rebuilt
        anim->set_int_variable(StringName("iState"), blend_state);
        break;
    }

    } // switch
}
