"""
Procedural Stylized Vegetation Generator for Blender 4.x
=========================================================
Run from Blender's Scripting workspace.

Generates N randomized variants of:
  - Trees (deciduous / pine)
  - Bushes
  - Grass clumps
  - Rocks

Each variant is a plain applied mesh in its own collection.
Tweak CONFIG at the top to control counts and parameters.
"""

import bpy
import bmesh
import random
import math
from mathutils import Vector, Matrix


# ---------------------------------------------------------------------------
# CONFIG
# ---------------------------------------------------------------------------

CONFIG = {
    "seed": 42,
    "variants": {
        "tree_deciduous": 4,
        "tree_pine":      3,
        "bush":           4,
        "grass":          4,
        "rock":           3,
    },
    "tree_deciduous": {
        "height_min":       2.5,
        "height_max":       4.5,
        "trunk_radius":     0.12,
        "branch_count_min": 4,
        "branch_count_max": 7,
        "canopy_radius_min":1.2,
        "canopy_radius_max":2.2,
        "canopy_segments":  6,      # low-poly ico subdivisions
        "canopy_layers":    2,      # stacked canopy blobs
    },
    "tree_pine": {
        "height_min":    3.0,
        "height_max":    5.5,
        "trunk_radius":  0.10,
        "tier_count_min":3,
        "tier_count_max":5,
        "tier_radius":   1.1,       # base tier radius
        "tier_shrink":   0.65,      # each tier shrinks by this factor
    },
    "bush": {
        "height_min":    0.5,
        "height_max":    1.2,
        "blob_count_min":3,
        "blob_count_max":6,
        "blob_radius_min":0.3,
        "blob_radius_max":0.6,
        "segments":      5,
    },
    "grass": {
        "blade_count_min": 5,
        "blade_count_max": 10,
        "blade_height_min":0.3,
        "blade_height_max":0.7,
        "spread":          0.3,
    },
    "rock": {
        "radius_min":    0.3,
        "radius_max":    0.8,
        "subdivisions":  2,
        "noise_scale":   0.6,
        "flatten":       0.5,       # y-scale to flatten the rock
    },
}

# ---------------------------------------------------------------------------
# UTILITIES
# ---------------------------------------------------------------------------

def clear_collection(name: str) -> bpy.types.Collection:
    if name in bpy.data.collections:
        col = bpy.data.collections[name]
        for obj in list(col.objects):
            bpy.data.objects.remove(obj, do_unlink=True)
        bpy.data.collections.remove(col)
    col = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(col)
    return col


def link_object(col: bpy.types.Collection, obj: bpy.types.Object):
    col.objects.link(obj)
    if obj.name in bpy.context.scene.collection.objects:
        bpy.context.scene.collection.objects.unlink(obj)


def apply_and_return(obj: bpy.types.Object) -> bpy.types.Object:
    """Apply all modifiers so result is a plain mesh."""
    ctx = bpy.context.copy()
    ctx["object"] = obj
    ctx["active_object"] = obj
    ctx["selected_objects"] = [obj]
    ctx["selected_editable_objects"] = [obj]
    bpy.context.view_layer.objects.active = obj
    for mod in list(obj.modifiers):
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


def new_mesh_object(name: str, mesh: bpy.types.Mesh) -> bpy.types.Object:
    obj = bpy.data.objects.new(name, mesh)
    return obj


def add_subsurf(obj, levels=1):
    mod = obj.modifiers.new("Subsurf", "SUBSURF")
    mod.levels = levels
    mod.render_levels = levels


def shade_flat(obj):
    for poly in obj.data.polygons:
        poly.use_smooth = False


# ---------------------------------------------------------------------------
# MATERIAL HELPERS  (simple vertex-color-ready diffuse materials)
# ---------------------------------------------------------------------------

def get_or_create_material(name: str, color: tuple) -> bpy.types.Material:
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.9
        bsdf.inputs["Specular IOR Level"].default_value = 0.0
    return mat


MATERIALS = {
    "bark":       (0.28, 0.18, 0.10),
    "leaf_green": (0.18, 0.42, 0.15),
    "leaf_pine":  (0.10, 0.30, 0.12),
    "grass":      (0.25, 0.55, 0.15),
    "bush":       (0.16, 0.38, 0.12),
    "rock":       (0.45, 0.42, 0.38),
}


def assign_material(obj, mat_name):
    mat = get_or_create_material(mat_name, MATERIALS[mat_name])
    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)


# ---------------------------------------------------------------------------
# DECIDUOUS TREE
# ---------------------------------------------------------------------------

def make_trunk_mesh(height: float, radius: float, segments: int = 6) -> bpy.types.Mesh:
    bm = bmesh.new()
    # Bottom circle
    bot_verts = []
    for i in range(segments):
        a = 2 * math.pi * i / segments
        v = bm.verts.new(Vector((math.cos(a) * radius, math.sin(a) * radius, 0.0)))
        bot_verts.append(v)
    # Top circle (tapered)
    top_r = radius * 0.4
    top_verts = []
    for i in range(segments):
        a = 2 * math.pi * i / segments
        v = bm.verts.new(Vector((math.cos(a) * top_r, math.sin(a) * top_r, height)))
        top_verts.append(v)
    # Side faces
    for i in range(segments):
        j = (i + 1) % segments
        bm.faces.new([bot_verts[i], bot_verts[j], top_verts[j], top_verts[i]])
    # Cap faces
    bmesh.ops.contextual_create(bm, geom=bot_verts)
    bmesh.ops.contextual_create(bm, geom=top_verts)
    bm.normal_update()
    me = bpy.data.meshes.new("trunk_mesh")
    bm.to_mesh(me)
    bm.free()
    return me


def make_canopy_blob(radius: float, segments: int, location: Vector) -> bpy.types.Mesh:
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=segments, radius=radius)
    # Flatten slightly for stylized look
    for v in bm.verts:
        v.co.z *= 0.75
    me = bpy.data.meshes.new("canopy_mesh")
    bm.to_mesh(me)
    bm.free()
    me.transform(Matrix.Translation(location))
    return me


def merge_meshes(name: str, meshes: list) -> bpy.types.Object:
    """Join multiple mesh data blocks into one object."""
    # Create objects for each mesh
    objects = []
    for me in meshes:
        o = bpy.data.objects.new("_tmp", me)
        bpy.context.scene.collection.objects.link(o)
        objects.append(o)

    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    result = bpy.context.active_object
    result.name = name
    bpy.context.scene.collection.objects.unlink(result)
    return result


def gen_tree_deciduous(rng: random.Random, idx: int) -> bpy.types.Object:
    cfg = CONFIG["tree_deciduous"]
    height    = rng.uniform(cfg["height_min"], cfg["height_max"])
    c_radius  = rng.uniform(cfg["canopy_radius_min"], cfg["canopy_radius_max"])
    b_count   = rng.randint(cfg["branch_count_min"], cfg["branch_count_max"])
    layers    = cfg["canopy_layers"]
    segs      = cfg["canopy_segments"]
    t_radius  = cfg["trunk_radius"]

    meshes = [make_trunk_mesh(height, t_radius)]

    # Canopy blobs
    for layer in range(layers):
        layer_z   = height * (0.75 + layer * 0.15)
        layer_r   = c_radius * (1.0 - layer * 0.25)
        blob_r    = layer_r * rng.uniform(0.7, 0.9)
        blob_loc  = Vector((rng.uniform(-0.1, 0.1), rng.uniform(-0.1, 0.1), layer_z))
        meshes.append(make_canopy_blob(blob_r, segs, blob_loc))

    # Side branch blobs
    for _ in range(b_count):
        angle  = rng.uniform(0, 2 * math.pi)
        dist   = rng.uniform(c_radius * 0.3, c_radius * 0.7)
        bz     = height * rng.uniform(0.65, 0.85)
        br     = c_radius * rng.uniform(0.3, 0.55)
        bloc   = Vector((math.cos(angle) * dist, math.sin(angle) * dist, bz))
        meshes.append(make_canopy_blob(br, max(1, segs - 1), bloc))

    obj = merge_meshes(f"Tree_Deciduous_{idx:02d}", meshes)

    # Split materials: slot 0 = bark (trunk), slot 1 = leaves (canopy)
    bark_mat = get_or_create_material("bark", MATERIALS["bark"])
    leaf_mat = get_or_create_material("leaf_green", MATERIALS["leaf_green"])
    obj.data.materials.append(bark_mat)
    obj.data.materials.append(leaf_mat)

    # Assign leaf material to upper half of faces by z centroid
    obj.data.update()
    for poly in obj.data.polygons:
        cx = sum(obj.data.vertices[vi].co.z for vi in poly.vertices) / len(poly.vertices)
        poly.material_index = 1 if cx > height * 0.5 else 0

    shade_flat(obj)
    return obj


# ---------------------------------------------------------------------------
# PINE TREE
# ---------------------------------------------------------------------------

def make_cone_tier(radius: float, height: float, z_base: float, segments: int = 7) -> bpy.types.Mesh:
    bm = bmesh.new()
    apex = bm.verts.new(Vector((0, 0, z_base + height)))
    base_verts = []
    for i in range(segments):
        a = 2 * math.pi * i / segments
        v = bm.verts.new(Vector((math.cos(a) * radius, math.sin(a) * radius, z_base)))
        base_verts.append(v)
    # Side faces
    for i in range(segments):
        j = (i + 1) % segments
        bm.faces.new([apex, base_verts[j], base_verts[i]])
    bmesh.ops.contextual_create(bm, geom=base_verts)
    bm.normal_update()
    me = bpy.data.meshes.new("cone_tier")
    bm.to_mesh(me)
    bm.free()
    return me


def gen_tree_pine(rng: random.Random, idx: int) -> bpy.types.Object:
    cfg     = CONFIG["tree_pine"]
    height  = rng.uniform(cfg["height_min"], cfg["height_max"])
    tiers   = rng.randint(cfg["tier_count_min"], cfg["tier_count_max"])
    t_rad   = cfg["trunk_radius"]

    meshes  = [make_trunk_mesh(height * 0.85, t_rad)]

    tier_height = height / tiers * 0.9
    for i in range(tiers):
        frac   = 1.0 - i / tiers
        t_r    = cfg["tier_radius"] * (frac ** 1.2) * rng.uniform(0.9, 1.1)
        z_base = height * (0.15 + i / tiers * 0.75)
        meshes.append(make_cone_tier(t_r, tier_height * frac, z_base))

    obj = merge_meshes(f"Tree_Pine_{idx:02d}", meshes)

    bark_mat     = get_or_create_material("bark", MATERIALS["bark"])
    leaf_mat     = get_or_create_material("leaf_pine", MATERIALS["leaf_pine"])
    obj.data.materials.append(bark_mat)
    obj.data.materials.append(leaf_mat)

    for poly in obj.data.polygons:
        cx = sum(obj.data.vertices[vi].co.z for vi in poly.vertices) / len(poly.vertices)
        poly.material_index = 1 if cx > height * 0.15 else 0

    shade_flat(obj)
    return obj


# ---------------------------------------------------------------------------
# BUSH
# ---------------------------------------------------------------------------

def gen_bush(rng: random.Random, idx: int) -> bpy.types.Object:
    cfg    = CONFIG["bush"]
    height = rng.uniform(cfg["height_min"], cfg["height_max"])
    blobs  = rng.randint(cfg["blob_count_min"], cfg["blob_count_max"])
    segs   = cfg["segments"]

    meshes = []
    for i in range(blobs):
        angle  = rng.uniform(0, 2 * math.pi)
        dist   = rng.uniform(0.0, height * 0.4)
        bz     = rng.uniform(height * 0.1, height * 0.6)
        br     = rng.uniform(cfg["blob_radius_min"], cfg["blob_radius_max"])
        loc    = Vector((math.cos(angle) * dist, math.sin(angle) * dist, bz))
        meshes.append(make_canopy_blob(br, segs, loc))

    obj = merge_meshes(f"Bush_{idx:02d}", meshes)
    assign_material(obj, "bush")
    shade_flat(obj)
    return obj


# ---------------------------------------------------------------------------
# GRASS CLUMP
# ---------------------------------------------------------------------------

def make_blade(height: float, lean_angle: float, lean_dir: float, base: Vector) -> bpy.types.Mesh:
    """Single grass blade as a thin triangle."""
    bm = bmesh.new()
    w  = 0.04
    tip_x = math.sin(lean_angle) * height * math.cos(lean_dir)
    tip_y = math.sin(lean_angle) * height * math.sin(lean_dir)
    tip_z = math.cos(lean_angle) * height

    v0 = bm.verts.new(base + Vector((-w, 0, 0)))
    v1 = bm.verts.new(base + Vector(( w, 0, 0)))
    v2 = bm.verts.new(base + Vector((tip_x, tip_y, tip_z)))
    bm.faces.new([v0, v1, v2])
    bm.normal_update()
    me = bpy.data.meshes.new("blade")
    bm.to_mesh(me)
    bm.free()
    return me


def gen_grass(rng: random.Random, idx: int) -> bpy.types.Object:
    cfg    = CONFIG["grass"]
    blades = rng.randint(cfg["blade_count_min"], cfg["blade_count_max"])
    spread = cfg["spread"]

    meshes = []
    for _ in range(blades):
        h     = rng.uniform(cfg["blade_height_min"], cfg["blade_height_max"])
        lean  = rng.uniform(0.0, 0.4)
        ldir  = rng.uniform(0, 2 * math.pi)
        bx    = rng.uniform(-spread, spread)
        by    = rng.uniform(-spread, spread)
        base  = Vector((bx, by, 0))
        meshes.append(make_blade(h, lean, ldir, base))

    obj = merge_meshes(f"Grass_{idx:02d}", meshes)
    assign_material(obj, "grass")
    shade_flat(obj)
    return obj


# ---------------------------------------------------------------------------
# ROCK
# ---------------------------------------------------------------------------

def gen_rock(rng: random.Random, idx: int) -> bpy.types.Object:
    cfg  = CONFIG["rock"]
    r    = rng.uniform(cfg["radius_min"], cfg["radius_max"])
    flat = cfg["flatten"]
    subdiv = cfg["subdivisions"]
    noise  = cfg["noise_scale"]

    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=r)

    # Random displacement per vertex
    for v in bm.verts:
        n     = v.normal.copy()
        jitter = rng.uniform(-noise * r * 0.3, noise * r * 0.3)
        v.co += n * jitter
        # Flatten Z
        v.co.z *= flat
        # Random XY squish for variety
        v.co.x *= rng.uniform(0.85, 1.15)
        v.co.y *= rng.uniform(0.85, 1.15)

    # Sink to ground
    min_z = min(v.co.z for v in bm.verts)
    for v in bm.verts:
        v.co.z -= min_z

    bm.normal_update()
    me = bpy.data.meshes.new("rock_mesh")
    bm.to_mesh(me)
    bm.free()

    obj = new_mesh_object(f"Rock_{idx:02d}", me)
    assign_material(obj, "rock")
    shade_flat(obj)
    return obj


# ---------------------------------------------------------------------------
# LAYOUT HELPERS
# ---------------------------------------------------------------------------

def arrange_in_grid(objects: list, spacing: float = 3.0):
    cols = max(1, math.ceil(math.sqrt(len(objects))))
    for i, obj in enumerate(objects):
        row = i // cols
        col = i % cols
        obj.location = Vector((col * spacing, row * spacing, 0.0))


# ---------------------------------------------------------------------------
# MAIN
# ---------------------------------------------------------------------------

def main():
    rng = random.Random(CONFIG["seed"])
    v   = CONFIG["variants"]

    root_col = clear_collection("Vegetation")

    generators = {
        "Trees_Deciduous": (gen_tree_deciduous, v["tree_deciduous"]),
        "Trees_Pine":      (gen_tree_pine,      v["tree_pine"]),
        "Bushes":          (gen_bush,           v["bush"]),
        "Grass":           (gen_grass,          v["grass"]),
        "Rocks":           (gen_rock,           v["rock"]),
    }

    for col_name, (gen_fn, count) in generators.items():
        sub_col = bpy.data.collections.new(col_name)
        root_col.children.link(sub_col)
        objects = []
        for i in range(count):
            if gen_fn == gen_rock:
                obj = gen_fn(rng, i)
                bpy.context.scene.collection.objects.link(obj)
            else:
                obj = gen_fn(rng, i)
                if obj.name not in [o.name for o in bpy.context.scene.collection.objects]:
                    bpy.context.scene.collection.objects.link(obj)
            link_object(sub_col, obj)
            objects.append(obj)
        arrange_in_grid(objects, spacing=4.0)
        # Offset each sub-collection row
        row_offset = list(generators.keys()).index(col_name)
        for obj in objects:
            obj.location.y += row_offset * 10.0

    print(f"[Vegetation] Done. Generated {sum(c for _, c in generators.values())} objects.")


main()
