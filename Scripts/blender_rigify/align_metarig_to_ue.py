"""
Run inside Blender's Scripting tab, AFTER adding a Rigify Human (Meta-Rig)
(Add > Armature > Human (Meta-Rig)) into the same scene as your imported
UE Mannequin skeleton, BEFORE clicking Rigify's "Generate Rig".

Snaps every mapped metarig bone's head/tail/roll onto the matching UE
Mannequin bone, so Generate Rig produces a control rig proportioned to
your character.

Bone-assignment safety, part 1 (aliasing): in Blender a connected child
bone's head (use_connect=True) is the SAME stored point as its parent's
tail, so assigning EditBone.head on a connected bone silently rewrites the
parent's tail. To avoid this, every metarig bone has use_connect
temporarily disabled before any position is touched (head/tail fully
independent, no aliasing), positions are assigned in one pass, then the
original use_connect flags are restored.

Bone-assignment safety, part 2 (parent tails): restoring use_connect=True
re-snaps each connected child's HEAD onto its parent's TAIL. That is only
harmless if the parent's assigned tail already sits at the child's head.
It does NOT, in general: a metarig parent is mapped to a UE bone, and
Blender's FBX importer only places a UE bone's tail at its child's head
when that bone has exactly one child. UE 'pelvis' and 'spine_03' are
multi-child bones, so their imported tails are arbitrary - keeping them
would yank spine.001 (torso base) and spine.004 (neck) onto the wrong point
on restore (detached/offset spine, busted rig). Fix: heads are the real
joint positions, so before assignment every connected bone overrides its
parent's target TAIL with its own target HEAD. Chains through single-child
UE bones (arms/legs/fingers) already coincide, so this only moves the two
genuinely-broken joints.

The spine/neck/head chain: the torso portion (pelvis, spine_01, spine_02,
spine_03) maps 1:1 onto the metarig's first 4 spine.NNN bones directly -
both skeletons have exactly 4 bones there. Each torso metarig bone takes
its HEAD from the matching UE bone but its TAIL from the NEXT joint's head,
building one clean connected polyline pelvis-head -> spine_01-head ->
spine_02-head -> spine_03-head -> neck_01-head. The UE bones' own tails are
NOT used for the torso: pelvis and spine_03 are multi-child (thigh_l/r and
neck_01+clavicle_l/r respectively), so the FBX importer gives them
arbitrary heuristic tails that would bend the metarig bone to a bogus
point. Driving tails from the next joint head instead makes the torso robust
regardless of whether the neck bone is use_connect'd to the chest (in some
Rigify versions spine.004 is NOT connected, so the reconcile pass below
cannot correct spine.003's tail). The neck vertebra(e) are handled with
proportional distribution, because some Rigify versions add an extra neck
vertebra (spine.004 AND spine.005 both in the neck, spine.006 = head) while
UE only has one neck bone (neck_01). The LAST metarig spine bone is ALWAYS
the head, so it maps 1:1 onto UE's head bone (its own head/tail/roll) and is
never smeared across the neck arc; only the neck vertebra(e) before it get
fitted proportionally (by relative original bone length) across the UE neck
polyline (neck_01[..neck_0N]).

Edit UE_ARMATURE and METARIG below if your object names differ.
"""

import bpy
from mathutils import Vector

UE_ARMATURE = "Armature"   # name of your imported UE skeleton object
METARIG = "metarig"        # name Blender gives the Human (Meta-Rig) by default

UE_TORSO_CHAIN = ["pelvis", "spine_01", "spine_02", "spine_03"]
UE_NECK_CHAIN = ["neck_01"]   # neck vertebra(e) only - UE5 manny would be ["neck_01", "neck_02"]
UE_HEAD_BONE = "head"          # the actual UE head bone, mapped 1:1 to the metarig's last spine bone
UE_SPINE_CHAIN = UE_TORSO_CHAIN + UE_NECK_CHAIN + [UE_HEAD_BONE]
METARIG_SPINE_ROOT = "spine"

# metarig_bone_name -> ue_bone_name (everything except the spine/neck/head chain,
# which is handled by build_spine_targets() below)
BONE_MAP = {
    "shoulder.L": "clavicle_l",
    "upper_arm.L": "upperarm_l",
    "forearm.L": "lowerarm_l",
    "hand.L": "hand_l",
    "shoulder.R": "clavicle_r",
    "upper_arm.R": "upperarm_r",
    "forearm.R": "lowerarm_r",
    "hand.R": "hand_r",

    "thigh.L": "thigh_l",
    "shin.L": "calf_l",
    "foot.L": "foot_l",
    "toe.L": "ball_l",
    "thigh.R": "thigh_r",
    "shin.R": "calf_r",
    "foot.R": "foot_r",
    "toe.R": "ball_r",

    "thumb.01.L": "thumb_01_l", "thumb.02.L": "thumb_02_l", "thumb.03.L": "thumb_03_l",
    "f_index.01.L": "index_01_l", "f_index.02.L": "index_02_l", "f_index.03.L": "index_03_l",
    "f_middle.01.L": "middle_01_l", "f_middle.02.L": "middle_02_l", "f_middle.03.L": "middle_03_l",
    "f_ring.01.L": "ring_01_l", "f_ring.02.L": "ring_02_l", "f_ring.03.L": "ring_03_l",
    "f_pinky.01.L": "pinky_01_l", "f_pinky.02.L": "pinky_02_l", "f_pinky.03.L": "pinky_03_l",

    "thumb.01.R": "thumb_01_r", "thumb.02.R": "thumb_02_r", "thumb.03.R": "thumb_03_r",
    "f_index.01.R": "index_01_r", "f_index.02.R": "index_02_r", "f_index.03.R": "index_03_r",
    "f_middle.01.R": "middle_01_r", "f_middle.02.R": "middle_02_r", "f_middle.03.R": "middle_03_r",
    "f_ring.01.R": "ring_01_r", "f_ring.02.R": "ring_02_r", "f_ring.03.R": "ring_03_r",
    "f_pinky.01.R": "pinky_01_r", "f_pinky.02.R": "pinky_02_r", "f_pinky.03.R": "pinky_03_r",
}

# palm.0N.L bones have no UE equivalent (UE parents fingers directly to hand_l).
# Bridge them from the hand bone to each finger's root so metarig topology stays valid.
PALM_MAP = {
    "palm.01.L": ("hand_l", "index_01_l"),
    "palm.02.L": ("hand_l", "middle_01_l"),
    "palm.03.L": ("hand_l", "ring_01_l"),
    "palm.04.L": ("hand_l", "pinky_01_l"),
    "palm.01.R": ("hand_r", "index_01_r"),
    "palm.02.R": ("hand_r", "middle_01_r"),
    "palm.03.R": ("hand_r", "ring_01_r"),
    "palm.04.R": ("hand_r", "pinky_01_r"),
}


def get_world_transforms(armature_name, bone_names):
    """Returns {bone_name: (world_head, world_tail, world_z_axis)} by entering edit mode once."""
    obj = bpy.data.objects[armature_name]
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")

    result = {}
    mat = obj.matrix_world
    for name in bone_names:
        eb = obj.data.edit_bones.get(name)
        if eb is None:
            print(f"[align_metarig_to_ue] WARNING: '{name}' not found in {armature_name}, skipping")
            continue
        head = mat @ eb.head
        tail = mat @ eb.tail
        z_axis = mat.to_3x3() @ eb.z_axis
        result[name] = (head, tail, z_axis)

    bpy.ops.object.mode_set(mode="OBJECT")
    return result


def detect_metarig_chain(edit_bones, root_name):
    """Walks root_name, root_name+'.001', '.002', ... as long as each bone exists."""
    chain = []
    if root_name not in edit_bones:
        return chain
    chain.append(root_name)
    i = 1
    while True:
        name = f"{root_name}.{i:03d}"
        if name not in edit_bones:
            break
        chain.append(name)
        i += 1
    return chain


def build_polyline(ue_transforms, chain_names):
    """Returns (points, seg_lengths, cum, total, z_axes) for a connected UE bone chain."""
    points = [ue_transforms[chain_names[0]][0]]
    z_axes = []
    for name in chain_names:
        points.append(ue_transforms[name][1])
        z_axes.append(ue_transforms[name][2])
    seg_lengths = [(points[i + 1] - points[i]).length for i in range(len(points) - 1)]
    cum = [0.0]
    for length in seg_lengths:
        cum.append(cum[-1] + length)
    return points, seg_lengths, cum, cum[-1], z_axes


def point_at_fraction(points, seg_lengths, cum, total, z_axes, frac):
    """Returns (world_position, world_z_axis) at arc-length fraction frac along the polyline."""
    target = frac * total
    for i, seg_len in enumerate(seg_lengths):
        if target <= cum[i + 1] or i == len(seg_lengths) - 1:
            local_t = 0.0 if seg_len < 1e-9 else (target - cum[i]) / seg_len
            local_t = max(0.0, min(1.0, local_t))
            pos = points[i] + (points[i + 1] - points[i]) * local_t
            return pos, z_axes[i]
    return points[-1], z_axes[-1]


def build_spine_targets(metarig, ue_transforms):
    """Returns {metarig_bone_name: (world_head, world_tail, world_z)} for the spine chain."""
    edit_bones = metarig.data.edit_bones
    chain = detect_metarig_chain(edit_bones, METARIG_SPINE_ROOT)
    if not chain:
        print(f"[align_metarig_to_ue] WARNING: metarig spine root '{METARIG_SPINE_ROOT}' not found")
        return {}

    targets = {}
    n_torso = len(UE_TORSO_CHAIN)

    # Torso: exact 1:1, both skeletons have exactly 4 bones here. Each torso
    # bone's HEAD is the matching UE bone head; its TAIL is the NEXT joint's
    # head - NOT the UE bone's own tail. Multi-child UE bones (pelvis has
    # thigh_l/r, spine_03 has neck_01 + clavicle_l/r) get an arbitrary
    # heuristic tail from the FBX importer, so copying it bends the bone to a
    # bogus point. The last torso bone (spine.003) tails onto neck_01's head,
    # which is exactly where the metarig neck chain (spine.004) starts, so the
    # torso and neck meet cleanly. This does not rely on the reconcile pass
    # below, which only corrects tails whose child bone is use_connect=True -
    # the torso->neck junction (spine.004) is not connected, so it would
    # otherwise keep spine_03's garbage tail.
    torso_chain = chain[:n_torso]
    joint_heads = []  # UE head of each torso bone, then neck_01 head as the terminal joint
    for ue_name in UE_TORSO_CHAIN:
        joint_heads.append(ue_transforms[ue_name][0] if ue_name in ue_transforms else None)
    neck_root = UE_NECK_CHAIN[0]
    joint_heads.append(ue_transforms[neck_root][0] if neck_root in ue_transforms else None)
    for i, (meta_name, ue_name) in enumerate(zip(torso_chain, UE_TORSO_CHAIN)):
        if ue_name not in ue_transforms or joint_heads[i + 1] is None:
            continue
        head, _, z_axis = ue_transforms[ue_name]
        targets[meta_name] = (head, joint_heads[i + 1], z_axis)

    # Neck/head: the metarig bones after the torso are neck vertebra(e)
    # followed by a single head bone. The LAST metarig spine bone is ALWAYS
    # the head, so map it 1:1 onto the UE head bone (its own head/tail/roll).
    # The remaining bones before it are neck vertebrae; distribute only those
    # proportionally (by original relative length) across the UE neck
    # polyline. Rigify versions vary in how many neck vertebrae they model
    # (one vs two), which is why the neck is proportional - but the head is
    # never smeared across the neck arc, so it lands exactly on UE's head.
    neck_and_head = chain[n_torso:]
    if neck_and_head:
        meta_head = neck_and_head[-1]
        meta_neck = neck_and_head[:-1]

        # Metarig head bone -> UE head bone, exact 1:1.
        if UE_HEAD_BONE in ue_transforms:
            targets[meta_head] = ue_transforms[UE_HEAD_BONE]
        else:
            print(f"[align_metarig_to_ue] WARNING: UE head bone '{UE_HEAD_BONE}' not found; "
                  f"metarig head '{meta_head}' left unaligned")

        # Metarig neck vertebra(e) proportionally across the UE neck polyline.
        if meta_neck:
            original_lengths = [edit_bones[name].length for name in meta_neck]
            total_original = sum(original_lengths)
            if total_original < 1e-9:
                print("[align_metarig_to_ue] WARNING: metarig neck chain has zero length, skipping")
                return targets

            points, seg_lengths, cum, total, z_axes = build_polyline(ue_transforms, UE_NECK_CHAIN)

            running = 0.0
            for name, orig_len in zip(meta_neck, original_lengths):
                head_frac = running / total_original
                running += orig_len
                tail_frac = running / total_original
                mid_frac = (head_frac + tail_frac) * 0.5

                world_head, _ = point_at_fraction(points, seg_lengths, cum, total, z_axes, head_frac)
                world_tail, _ = point_at_fraction(points, seg_lengths, cum, total, z_axes, tail_frac)
                _, world_z = point_at_fraction(points, seg_lengths, cum, total, z_axes, mid_frac)
                targets[name] = (world_head, world_tail, world_z)

    print(f"[align_metarig_to_ue] Fitted {len(chain)} metarig spine bones "
          f"({chain[0]} .. {chain[-1]}): {n_torso} torso bones 1:1, "
          f"{len(neck_and_head) - 1} neck bones onto {UE_NECK_CHAIN}, "
          f"head bone '{neck_and_head[-1]}' 1:1 onto '{UE_HEAD_BONE}'.")
    return targets


def main():
    ue_bone_names = set(BONE_MAP.values()) | set(UE_SPINE_CHAIN)
    for hand_bone, finger_root in PALM_MAP.values():
        ue_bone_names.add(hand_bone)
        ue_bone_names.add(finger_root)

    ue_transforms = get_world_transforms(UE_ARMATURE, ue_bone_names)

    metarig = bpy.data.objects[METARIG]
    bpy.context.view_layer.objects.active = metarig
    bpy.ops.object.mode_set(mode="EDIT")
    inv = metarig.matrix_world.inverted()
    inv3 = inv.to_3x3()
    edit_bones = metarig.data.edit_bones

    # Disable use_connect on every bone before touching positions, so head
    # and tail are fully independent (no parent-tail aliasing) during
    # assignment. Restored at the end.
    original_connect = {eb.name: eb.use_connect for eb in edit_bones}
    for eb in edit_bones:
        eb.use_connect = False

    targets = {}
    targets.update(build_spine_targets(metarig, ue_transforms))

    for meta_name, ue_name in BONE_MAP.items():
        if meta_name not in edit_bones or ue_name not in ue_transforms:
            print(f"[align_metarig_to_ue] WARNING: skipping {meta_name} -> {ue_name}")
            continue
        targets[meta_name] = ue_transforms[ue_name]

    for meta_name, (hand_bone, finger_root) in PALM_MAP.items():
        if meta_name not in edit_bones:
            continue
        hand_head, hand_tail, hand_z = ue_transforms[hand_bone]
        finger_head, _, _ = ue_transforms[finger_root]
        targets[meta_name] = (hand_tail, finger_head, hand_z)

    # Reconcile connected chains: a connected child's head IS its parent's tail
    # (that's what use_connect means). The UE bone a parent maps to often has a
    # tail that does NOT sit at the child's head - Blender's FBX importer only
    # places a bone's tail at its child's head when that bone has exactly ONE
    # child. UE 'pelvis' (children spine_01/thigh_l/thigh_r) and 'spine_03'
    # (children neck_01/clavicle_l/clavicle_r) are multi-child, so their imported
    # tails are arbitrary heuristic points. If we kept those tails, restoring
    # use_connect below would snap spine.001 (torso base) and spine.004 (neck)
    # onto them -> detached/offset spine, busted rig. Heads are the real joint
    # positions, so for every originally-connected bone we override its PARENT's
    # target tail with this bone's target head. Bones through single-child UE
    # parents (arms/legs/fingers) already coincide, so this is a no-op for them.
    for name, was_connected in original_connect.items():
        if not was_connected:
            continue
        eb = edit_bones.get(name)
        if eb is None or eb.parent is None:
            continue
        parent_name = eb.parent.name
        if name not in targets or parent_name not in targets:
            continue
        child_head = targets[name][0]
        p_head, _, p_z = targets[parent_name]
        targets[parent_name] = (p_head, child_head, p_z)

    for name, (world_head, world_tail, world_z) in targets.items():
        eb = edit_bones[name]
        eb.head = inv @ world_head
        eb.tail = inv @ world_tail
        eb.align_roll(inv3 @ world_z)

    for name, was_connected in original_connect.items():
        eb = edit_bones.get(name)
        if eb:
            eb.use_connect = was_connected

    bpy.ops.object.mode_set(mode="OBJECT")
    print(f"[align_metarig_to_ue] Aligned {len(targets)} metarig bones to {UE_ARMATURE}.")
    print("Now run prune_metarig_extras.py, then click Rigify > Generate Rig.")


main()
