import sys
import traceback
from pathlib import Path

import bpy

sys.path.insert(0, str(Path(__file__).parent))
import foliage_generator as fg

fg.register()

atlas_json = str(Path(__file__).parent / "d6b0cc6c-b2bc-4eb8-9a96-3d8a6bb79de6_atlas.json")

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()

try:
    bpy.ops.mesh.generate_foliage(atlas_json=atlas_json, mode='TREE', seed=3, height=4.5,
                                   branch_levels=4, canopy_shape='SPHERE')
    tree = bpy.data.objects["Foliage_Tree"]
    tree.location = (-2.5, 0, 0)
except Exception:
    traceback.print_exc()
    raise

try:
    bpy.ops.mesh.generate_foliage(atlas_json=atlas_json, mode='BUSH', seed=7, height=1.4,
                                   branch_levels=3, foliage_start_depth=0, canopy_shape='SPHERE',
                                   card_size=0.14)
    bush = bpy.data.objects["Foliage_Bush"]
    bush.location = (2.0, 0, 0)
except Exception:
    traceback.print_exc()
    raise

# ground plane
bpy.ops.mesh.primitive_plane_add(size=20, location=(0, 0, 0))

# sun
sun = bpy.data.lights.new("Sun", type='SUN')
sun.energy = 3.0
sun_obj = bpy.data.objects.new("Sun", sun)
bpy.context.collection.objects.link(sun_obj)
sun_obj.rotation_euler = (0.9, 0.2, 0.6)

# camera
cam = bpy.data.cameras.new("Cam")
cam_obj = bpy.data.objects.new("Cam", cam)
bpy.context.collection.objects.link(cam_obj)
cam_obj.location = (0, -9, 3.2)
cam_obj.rotation_euler = (1.35, 0, 0)
bpy.context.scene.camera = cam_obj

scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items] else 'BLENDER_EEVEE'
scene.render.resolution_x = 1000
scene.render.resolution_y = 800
scene.render.filepath = str(Path(__file__).parent / "_test_render.png")
bpy.ops.render.render(write_still=True)

blend_path = str(Path(__file__).parent / "_test_scene.blend")
bpy.ops.wm.save_as_mainfile(filepath=blend_path)

print("OK - render + blend written")
