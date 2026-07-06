"""
Run this AFTER align_metarig_to_ue.py to print diagnostic info if the
result looks wrong (spine offset, hands messed up, shoulders twisted, etc).
Paste the printed output back for debugging - don't need to interpret it
yourself.

Reports, per bone pair, world-space head/tail AND the bone's world-space
roll axis (local +Z / z_axis) plus the raw roll in degrees, so orientation
("facing wrong ways") problems are visible as a z-axis mismatch even when
head/tail line up perfectly.

Edit UE_ARMATURE / METARIG below if your object names differ.
"""

import bpy
from math import degrees

UE_ARMATURE = "Armature"
METARIG = "metarig"

# (ue_bone, meta_bone). meta_bone None => informational, printed UE-only.
# spine.005/spine.006/head are resolved against the detected chain below.
CHECK_BONES_UE_TO_META = [
    ("pelvis", "spine"),
    ("spine_01", "spine.001"),
    ("spine_03", "spine.003"),
    ("neck_01", "spine.004"),
    ("neck_01", "spine.005"),      # 2nd neck vertebra (some Rigify versions)
    ("head", "HEAD"),              # HEAD = sentinel: last spine.NNN bone
    ("clavicle_l", "shoulder.L"),
    ("clavicle_r", "shoulder.R"),
    ("upperarm_l", "upper_arm.L"),
    ("hand_l", "hand.L"),
    ("hand_l", "palm.01.L"),       # UE has no palm; bridged hand->finger by design
    ("index_01_l", "f_index.01.L"),
    ("thumb_01_l", "thumb.01.L"),
]


def obj_transform_report(obj):
    loc, rot, scale = obj.matrix_world.decompose()
    return (f"  loc={tuple(round(v, 4) for v in loc)} "
            f"rot_quat={tuple(round(v, 4) for v in rot)} "
            f"scale={tuple(round(v, 4) for v in scale)}")


def collect_edit_bone_data(obj, bone_names):
    """Enter edit mode once, return {name: (world_head, world_tail, world_z, roll_deg)}."""
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    mat = obj.matrix_world
    mat3 = mat.to_3x3()
    out = {}
    for name in bone_names:
        eb = obj.data.edit_bones.get(name)
        if eb is None:
            continue
        out[name] = (mat @ eb.head, mat @ eb.tail, mat3 @ eb.z_axis, degrees(eb.roll))
    bpy.ops.object.mode_set(mode="OBJECT")
    return out


def fmt(v):
    return tuple(round(c, 4) for c in v)


def main():
    ue_obj = bpy.data.objects[UE_ARMATURE]
    metarig = bpy.data.objects[METARIG]

    # Resolve the metarig spine chain so HEAD sentinel -> real last bone name.
    bpy.context.view_layer.objects.active = metarig
    bpy.ops.object.mode_set(mode="EDIT")
    ebs = metarig.data.edit_bones
    chain = ["spine"] if "spine" in ebs else []
    i = 1
    while f"spine.{i:03d}" in ebs:
        chain.append(f"spine.{i:03d}")
        i += 1
    bpy.ops.object.mode_set(mode="OBJECT")
    meta_head_name = chain[-1] if chain else None

    checks = []
    for ue_name, meta_name in CHECK_BONES_UE_TO_META:
        if meta_name == "HEAD":
            meta_name = meta_head_name
        checks.append((ue_name, meta_name))

    ue_data = collect_edit_bone_data(ue_obj, [c[0] for c in checks])
    meta_data = collect_edit_bone_data(metarig, [c[1] for c in checks if c[1]])

    print("=== Object transforms ===")
    print(f"{UE_ARMATURE}:")
    print(obj_transform_report(ue_obj))
    print(f"{METARIG}:")
    print(obj_transform_report(metarig))

    print("\n=== Bone comparison (world space, meters; roll axis = local +Z) ===")
    for ue_name, meta_name in checks:
        print(f"\n{ue_name}  vs  {meta_name}")
        ue = ue_data.get(ue_name)
        meta = meta_data.get(meta_name) if meta_name else None
        if ue is None:
            print(f"  UE bone '{ue_name}' not found")
        else:
            print(f"  UE    head={fmt(ue[0])} tail={fmt(ue[1])} zaxis={fmt(ue[2])} roll={ue[3]:.1f}deg")
        if meta is None:
            print(f"  meta bone '{meta_name}' not found")
        else:
            print(f"  meta  head={fmt(meta[0])} tail={fmt(meta[1])} zaxis={fmt(meta[2])} roll={meta[3]:.1f}deg")
        if ue and meta:
            dh = meta[0] - ue[0]
            zdot = ue[2].normalized().dot(meta[2].normalized())
            print(f"  head delta len={dh.length:.4f}   zaxis dot={zdot:.3f}  "
                  f"(1=aligned roll, -1=flipped 180deg, 0=perpendicular)")

    print(f"\nDetected metarig spine chain ({len(chain)} bones): {chain}")


main()
