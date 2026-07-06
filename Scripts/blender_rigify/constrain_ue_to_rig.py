"""
Run inside Blender's Scripting tab AFTER clicking Rigify's "Generate Rig"
button (this creates a new armature object, default name "rig").

Adds a Copy Transforms constraint on every deform bone in your ORIGINAL
UE skeleton, targeting the matching DEF- bone on the generated Rigify rig.
Your UE armature never has its bone names/hierarchy changed, so it stays
safe to export back to Unreal once you bake.

The spine/neck/head chain is handled dynamically rather than a fixed name
map: Rigify versions differ in how many spine.NNN bones the neck/head
region uses (see align_metarig_to_ue.py), so DEF-spine.NNN bones are
auto-detected and matched to UE chain bones by proportional arc-length
overlap instead of assuming e.g. DEF-spine.004 == neck_01.

Workflow after running this:
  1. Animate using the generated "rig" object's controls (IK/FK, poles, etc).
  2. Select the UE armature -> Pose > Animation > Bake Action
     (Visual Keying ON, Clear Constraints ON, "Only Selected Bones" as needed).
  3. Export the UE armature + mesh to FBX as usual. The "rig" object and
     metarig can be excluded from export or deleted.

Edit UE_ARMATURE / GENERATED_RIG below if your object names differ.
"""

import bpy

UE_ARMATURE = "Armature"
GENERATED_RIG = "rig"  # Rigify's default name for the generated control armature

UE_SPINE_CHAIN = ["pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head"]
DEF_SPINE_ROOT = "DEF-spine"

# ue_bone_name -> generated rig DEF- bone name (everything except the
# spine/neck/head chain, which fit_spine_constraints() handles below)
# Rigify auto-generates twist/tweak deform bones (e.g. DEF-upper_arm.L.001)
# for limb twist, which map directly onto UE's *_twist_01 bones.
DEF_MAP = {
    "clavicle_l": "DEF-shoulder.L",
    "upperarm_l": "DEF-upper_arm.L",
    "upperarm_twist_01_l": "DEF-upper_arm.L.001",
    "lowerarm_l": "DEF-forearm.L",
    "lowerarm_twist_01_l": "DEF-forearm.L.001",
    "hand_l": "DEF-hand.L",

    "clavicle_r": "DEF-shoulder.R",
    "upperarm_r": "DEF-upper_arm.R",
    "upperarm_twist_01_r": "DEF-upper_arm.R.001",
    "lowerarm_r": "DEF-forearm.R",
    "lowerarm_twist_01_r": "DEF-forearm.R.001",
    "hand_r": "DEF-hand.R",

    "thigh_l": "DEF-thigh.L",
    "thigh_twist_01_l": "DEF-thigh.L.001",
    "calf_l": "DEF-shin.L",
    "calf_twist_01_l": "DEF-shin.L.001",
    "foot_l": "DEF-foot.L",
    "ball_l": "DEF-toe.L",

    "thigh_r": "DEF-thigh.R",
    "thigh_twist_01_r": "DEF-thigh.R.001",
    "calf_r": "DEF-shin.R",
    "calf_twist_01_r": "DEF-shin.R.001",
    "foot_r": "DEF-foot.R",
    "ball_r": "DEF-toe.R",

    "thumb_01_l": "DEF-thumb.01.L", "thumb_02_l": "DEF-thumb.02.L", "thumb_03_l": "DEF-thumb.03.L",
    "index_01_l": "DEF-f_index.01.L", "index_02_l": "DEF-f_index.02.L", "index_03_l": "DEF-f_index.03.L",
    "middle_01_l": "DEF-f_middle.01.L", "middle_02_l": "DEF-f_middle.02.L", "middle_03_l": "DEF-f_middle.03.L",
    "ring_01_l": "DEF-f_ring.01.L", "ring_02_l": "DEF-f_ring.02.L", "ring_03_l": "DEF-f_ring.03.L",
    "pinky_01_l": "DEF-f_pinky.01.L", "pinky_02_l": "DEF-f_pinky.02.L", "pinky_03_l": "DEF-f_pinky.03.L",

    "thumb_01_r": "DEF-thumb.01.R", "thumb_02_r": "DEF-thumb.02.R", "thumb_03_r": "DEF-thumb.03.R",
    "index_01_r": "DEF-f_index.01.R", "index_02_r": "DEF-f_index.02.R", "index_03_r": "DEF-f_index.03.R",
    "middle_01_r": "DEF-f_middle.01.R", "middle_02_r": "DEF-f_middle.02.R", "middle_03_r": "DEF-f_middle.03.R",
    "ring_01_r": "DEF-f_ring.01.R", "ring_02_r": "DEF-f_ring.02.R", "ring_03_r": "DEF-f_ring.03.R",
    "pinky_01_r": "DEF-f_pinky.01.R", "pinky_02_r": "DEF-f_pinky.02.R", "pinky_03_r": "DEF-f_pinky.03.R",
}

# UE-only IK sockets (ik_foot_root, ik_hand_gun, ...) are intentionally left
# out of DEF_MAP: they're Unreal's own runtime IK bones, not part of the
# mesh deform chain, and have no Rigify equivalent. Leave them un-animated
# (or hand-key ik_hand_gun for weapon-attach previews) - see chat notes.


def detect_chain(bones, root_name):
    """Walks root_name, root_name+'.001', '.002', ... as long as each bone exists."""
    chain = []
    if root_name not in bones:
        return chain
    chain.append(root_name)
    i = 1
    while True:
        name = f"{root_name}.{i:03d}"
        if name not in bones:
            break
        chain.append(name)
        i += 1
    return chain


def chain_fractions(bones, chain):
    """Returns [(name, head_frac, tail_frac), ...] by cumulative bone length."""
    lengths = [bones[name].length for name in chain]
    total = sum(lengths)
    out = []
    running = 0.0
    for name, length in zip(chain, lengths):
        head_frac = running / total if total > 1e-9 else 0.0
        running += length
        tail_frac = running / total if total > 1e-9 else 0.0
        out.append((name, head_frac, tail_frac))
    return out


def overlap(a1, a2, b1, b2):
    return max(0.0, min(a2, b2) - max(a1, b1))


def fit_spine_constraints(ue_obj, rig_obj):
    def_chain = detect_chain(rig_obj.data.bones, DEF_SPINE_ROOT)
    if not def_chain:
        print(f"[constrain_ue_to_rig] WARNING: '{DEF_SPINE_ROOT}' not found on {GENERATED_RIG}")
        return 0

    def_fracs = chain_fractions(rig_obj.data.bones, def_chain)
    ue_fracs = chain_fractions(ue_obj.data.bones, UE_SPINE_CHAIN)

    applied = 0
    for ue_name, ue_head, ue_tail in ue_fracs:
        best_name, best_overlap = None, -1.0
        for def_name, def_head, def_tail in def_fracs:
            ov = overlap(ue_head, ue_tail, def_head, def_tail)
            if ov > best_overlap:
                best_overlap, best_name = ov, def_name

        pbone = ue_obj.pose.bones.get(ue_name)
        if pbone is None or best_name is None:
            continue

        existing = pbone.constraints.get("RigifyFollow")
        if existing:
            pbone.constraints.remove(existing)

        con = pbone.constraints.new("COPY_TRANSFORMS")
        con.name = "RigifyFollow"
        con.target = rig_obj
        con.subtarget = best_name
        applied += 1
        print(f"[constrain_ue_to_rig] spine chain: '{ue_name}' <- '{best_name}'")

    return applied


def main():
    ue_obj = bpy.data.objects[UE_ARMATURE]
    rig_obj = bpy.data.objects[GENERATED_RIG]

    applied = fit_spine_constraints(ue_obj, rig_obj)

    for ue_bone, def_bone in DEF_MAP.items():
        pbone = ue_obj.pose.bones.get(ue_bone)
        if pbone is None:
            print(f"[constrain_ue_to_rig] WARNING: '{ue_bone}' not found on {UE_ARMATURE}")
            continue
        if def_bone not in rig_obj.pose.bones:
            print(f"[constrain_ue_to_rig] WARNING: '{def_bone}' not found on {GENERATED_RIG}")
            continue

        existing = pbone.constraints.get("RigifyFollow")
        if existing:
            pbone.constraints.remove(existing)

        con = pbone.constraints.new("COPY_TRANSFORMS")
        con.name = "RigifyFollow"
        con.target = rig_obj
        con.subtarget = def_bone
        applied += 1

    print(f"[constrain_ue_to_rig] Added {applied} Copy Transforms constraints "
          f"from '{UE_ARMATURE}' to '{GENERATED_RIG}'.")
    print("Animate using the 'rig' controls, then bake + clear constraints on "
          f"'{UE_ARMATURE}' before exporting.")

    mapped_def_bones = set(DEF_MAP.values()) | {name for name, _, _ in chain_fractions(
        rig_obj.data.bones, detect_chain(rig_obj.data.bones, DEF_SPINE_ROOT))}
    leftover = [b.name for b in rig_obj.pose.bones
                if b.name.startswith("DEF-") and b.name not in mapped_def_bones]
    if leftover:
        print(f"[constrain_ue_to_rig] NOTE: {len(leftover)} DEF- bones on '{GENERATED_RIG}' "
              "have no UE Mannequin counterpart (run prune_metarig_extras.py before "
              "Generate Rig to avoid these, or ignore - unused DEF- bones with no "
              "vertex groups don't affect the mesh):")
        for name in leftover:
            print(f"    {name}")


main()
