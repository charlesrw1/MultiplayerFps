Rigify control rig for the UE Mannequin skeleton
=================================================

Purpose: animate the imported UE Mannequin skeleton in Blender using a full
Rigify FK/IK control rig, while keeping the original skeleton's bone names
and hierarchy untouched so it can be exported straight back to Unreal.

All scripts run inside Blender's Scripting tab (Scripting workspace tab,
paste script into a new text block, Run Script / Alt+P). Edit the
UE_ARMATURE / METARIG / GENERATED_RIG constants at the top of each script
first if your object names differ from the defaults ("Armature",
"metarig", "rig").

Run in this order:

1. Import your UE Mannequin FBX. Note its armature object name.

2. Add > Armature > Human (Meta-Rig). This adds an object named "metarig".

3. Run align_metarig_to_ue.py
   Snaps every metarig bone (spine, arms, legs, all fingers) onto the
   matching bone on your UE armature (head/tail/roll), using the standard
   UE Mannequin bone names (pelvis, spine_01-03, clavicle_l/r,
   upperarm/lowerarm/hand_l/r, thigh/calf/foot/ball_l/r, finger chains).
   palm.0N.L/R metarig bones (UE has no palm bones) are bridged from
   hand_l/r to each finger's root bone so the metarig topology stays valid.

4. Run prune_metarig_extras.py
   Deletes metarig bone chains with no UE Mannequin equivalent: the full
   face rig ("face" and ~50 descendant bones), breast.L/R, and
   pelvis.L/R (hip helper bones). Keeps heel.02.L/R - not a deform bone,
   only used internally by the leg rig type to build the foot-roll IK
   control.

5. In the metarig's Armature Properties tab, click Rigify > Generate Rig.
   Produces a new object (default name "rig") with full FK/IK controls
   plus a DEF- deform bone layer, including auto-generated twist/tweak
   bones (e.g. DEF-upper_arm.L.001) that correspond to UE's *_twist_01
   bones.

6. Run constrain_ue_to_rig.py
   Adds a Copy Transforms constraint (named "RigifyFollow") on every
   deform bone of your ORIGINAL UE armature, targeting the matching DEF-
   bone on "rig". Your UE armature's names/hierarchy never change. Also
   prints any DEF- bones on "rig" that weren't mapped (should be empty if
   step 4 ran correctly) - unused DEF- bones with no vertex groups don't
   affect the mesh, but flag them if you see any so you know something
   about your Rigify/Blender version differs from what these scripts
   assume.

7. Animate using the generated "rig" object's controls (IK hands/feet,
   pole targets, FK arms, finger curls). The UE armature follows live.

8. Before export: select the UE armature, Pose > Animation > Bake Action
   (Visual Keying ON, Clear Constraints ON). This bakes the followed
   motion into real keyframes on the original bones and removes the
   RigifyFollow constraints. Export the UE armature + mesh to FBX as
   usual; exclude/delete the metarig and "rig" objects from the export.

Notes
-----
- ik_foot_root/ik_foot_l/ik_foot_r/ik_hand_root/ik_hand_gun/ik_hand_l/r
  on the UE skeleton are Unreal's own runtime IK sockets, not part of the
  mesh deform chain. They have no Rigify equivalent and are intentionally
  left un-animated by these scripts. Hand-key ik_hand_gun if you want a
  weapon-attach preview in Blender, but it isn't required for export.
- If a limb looks twisted right after Generate Rig, it's almost always a
  roll mismatch from step 3 - re-check that the source UE bone's roll
  looks correct in Edit Mode before re-running the align script.
- "RIGIFY ERROR: Bone 'spine.NNN': Cannot connect chain - bone position is
  disjoint" on Generate Rig means the spine/neck/head chain wasn't fully
  aligned (older versions of align_metarig_to_ue.py hardcoded the neck bone
  count, which breaks on Rigify versions with an extra neck vertebra). The
  current align_metarig_to_ue.py auto-detects however many spine.NNN bones
  the metarig actually has and fits them all proportionally onto
  pelvis -> spine_01 -> spine_02 -> spine_03 -> neck_01 -> head, so this
  shouldn't happen - if it still does, delete the metarig, re-add
  Human (Meta-Rig), and re-run steps 3-4 from scratch.
- Torso base and/or neck detached or offset after align_metarig_to_ue.py
  (spine.001 or spine.004 yanked away from the rest of the spine): caused by
  restoring use_connect, which snaps a connected child's head onto its
  parent's tail. A metarig parent maps to a UE bone, but Blender's FBX
  importer only puts a UE bone's tail at its child's head when that bone has
  exactly ONE child. UE 'pelvis' (children spine_01/thigh_l/thigh_r) and
  'spine_03' (children neck_01/clavicle_l/clavicle_r) are multi-child, so
  their imported tails sit at arbitrary heuristic points - snapping the child
  onto them breaks the chain. Fixed: the script now drives every connected
  bone's PARENT tail from the child's HEAD (the real joint position) before
  assigning, so restoring use_connect is a true no-op. If you still see this,
  you're on an older copy of the script - re-pull it.
- Finger tails pointing the wrong way / elbow/hand wildly wrong: a separate
  aliasing gotcha (a connected child's head is the same stored point as its
  parent's tail, so assigning the head rewrites the tail). Handled by
  disabling use_connect on every metarig bone before touching positions,
  assigning in one pass, then restoring the flags.
- Spine bones positioned correctly at the shoulders/hands but drifting
  further off the further up the torso you look: caused by distributing
  ALL metarig spine bones (pelvis through head) proportionally by relative
  bone length across the whole UE chain, which only lines up if both
  skeletons happen to have identical segment proportions. Fixed by mapping
  the torso (spine, spine.001, spine.002, spine.003) 1:1 directly onto UE's
  pelvis/spine_01/spine_02/spine_03 (both always have exactly 4 bones
  there) and reserving the proportional-fit logic for the neck/head portion
  only, where bone counts actually can differ between Rigify versions.
- Re-running align_metarig_to_ue.py + prune_metarig_extras.py on a fresh
  metarig is safe to repeat if you need to regenerate the rig from
  scratch (e.g. after a Rigify update).
