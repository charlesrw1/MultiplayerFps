"""
Run inside Blender's Scripting tab AFTER align_metarig_to_ue.py,
BEFORE clicking Rigify's "Generate Rig".

The default Rigify Human Meta-Rig includes a full face rig, breast bones,
and hip helper bones that have no equivalent on the UE Mannequin skeleton.
Left in place, Generate Rig produces extra DEF- bones with nothing in the
Mannequin to drive - this deletes those bone chains from the metarig first.

Bones that ARE kept even though they don't map to a UE bone:
  - heel.02.L/R: not a deform bone, only used internally by the leg rig
    type to build the foot-roll IK control. Deleting it breaks leg IK setup.

Edit METARIG below if your object name differs.
"""

import bpy

METARIG = "metarig"

# Root bones of chains with no UE Mannequin equivalent. Their full
# descendant trees are removed too (e.g. "face" takes ~50 sub-bones with it).
PRUNE_ROOTS = [
    "face",
    "breast.L",
    "breast.R",
    "pelvis.L",
    "pelvis.R",
]


def collect_with_descendants(edit_bones, root_names):
    to_remove = set()
    for root_name in root_names:
        root = edit_bones.get(root_name)
        if root is None:
            continue
        stack = [root]
        while stack:
            b = stack.pop()
            to_remove.add(b.name)
            stack.extend(b.children)
    return to_remove


def main():
    metarig = bpy.data.objects[METARIG]
    bpy.context.view_layer.objects.active = metarig
    bpy.ops.object.mode_set(mode="EDIT")

    edit_bones = metarig.data.edit_bones
    names = collect_with_descendants(edit_bones, PRUNE_ROOTS)

    if not names:
        print("[prune_metarig_extras] Nothing matched PRUNE_ROOTS - already pruned?")
        bpy.ops.object.mode_set(mode="OBJECT")
        return

    # Remove leaves first so nothing gets reparented mid-loop unexpectedly.
    ordered = sorted(names, key=lambda n: len(edit_bones[n].parent_recursive) if edit_bones.get(n) else 0, reverse=True)
    for name in ordered:
        eb = edit_bones.get(name)
        if eb:
            edit_bones.remove(eb)

    bpy.ops.object.mode_set(mode="OBJECT")
    print(f"[prune_metarig_extras] Removed {len(names)} metarig bones "
          f"(face rig, breasts, hip helpers) with no UE Mannequin equivalent.")
    print("Now click Rigify > Generate Rig.")


main()
