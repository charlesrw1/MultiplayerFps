"""
Procedural bush/tree generator using hand-painted foliage card cutouts.

Install as a Blender addon (Edit > Preferences > Add-ons > Install...) or
just open it in the Scripting tab and hit Run Script - either way it
registers "Add > Mesh > Foliage (Cards)" and a Foliage tab in the 3D
viewport sidebar (N-panel).

The operator exposes ~30 parameters as real bpy.props, so every run gets
an "Adjust Last Operation" panel (bottom-left / F9) with live sliders -
seed, branch structure, canopy shape, card density/mix/tint - all
re-generate in place without re-running anything by hand.

Pipeline:
  1. Run prep_foliage_atlas.py once on a reference sheet to get
     <name>_atlas.png + <name>_atlas.json (cut-out cards + uv rects,
     classified as leaf / cluster / branch / twig / acorn).
  2. Point the Foliage panel at that .json.
  3. Add > Mesh > Foliage (Cards), pick Bush or Tree, tweak sliders.

Everything (skeleton + cards) is baked into two real mesh objects per
plant (Trunk, Foliage) so it's fast to render and easy to bake/export -
no geometry nodes dependency, no particle systems.
"""
bl_info = {
    "name": "Foliage Card Generator",
    "author": "CsRemake",
    "version": (1, 0, 0),
    "blender": (4, 0, 0),
    "location": "View3D > Add > Mesh, View3D > Sidebar > Foliage",
    "description": "Procedural bush/tree generator built from foliage card cutouts",
    "category": "Add Mesh",
}

import json
import math
import random
from pathlib import Path

import bpy
import bmesh
from mathutils import Vector, Matrix, Quaternion
from bpy.props import (
    StringProperty, FloatProperty, IntProperty, EnumProperty, BoolProperty,
)

_ATLAS_CACHE = {}


def load_atlas(json_path: str):
    if not json_path:
        return None
    cached = _ATLAS_CACHE.get(json_path)
    mtime = Path(json_path).stat().st_mtime if Path(json_path).exists() else None
    if cached and cached[0] == mtime:
        return cached[1]
    if mtime is None:
        return None
    data = json.loads(Path(json_path).read_text())
    by_kind = {}
    for c in data["cards"]:
        by_kind.setdefault(c["kind"], []).append(c)
    data["by_kind"] = by_kind
    data["json_dir"] = str(Path(json_path).parent)
    _ATLAS_CACHE[json_path] = (mtime, data)
    return data


# ---------------------------------------------------------------------------
# skeleton (trunk / branches)
# ---------------------------------------------------------------------------

class Segment:
    __slots__ = ("start", "end", "r0", "r1", "depth", "parent")

    def __init__(self, start, end, r0, r1, depth, parent=None):
        self.start = start
        self.end = end
        self.r0 = r0
        self.r1 = r1
        self.depth = depth
        self.parent = parent


def canopy_bias(pos, height, shape, strength, depth, max_depth):
    """Blend the growth direction toward a canopy envelope so tips fill out
    a recognisable silhouette instead of growing unbounded."""
    if strength <= 0.0 or depth < 1:
        return Vector((0, 0, 0))
    t = depth / max(1, max_depth)
    center = Vector((0, 0, height * (0.55 if shape != "VASE" else 0.75)))
    to_center = center - pos
    if shape == "CONE":
        radius_at_h = max(0.05, (height - pos.z) / height) * height * 0.35
    elif shape == "VASE":
        radius_at_h = height * 0.4 * math.sin(min(1.0, pos.z / height) * math.pi * 0.5 + 0.3)
    else:  # SPHERE
        radius_at_h = height * 0.32

    horiz = Vector((pos.x - center.x, pos.y - center.y, 0))
    dist = horiz.length
    if dist > radius_at_h:
        pull = horiz.normalized() * -1.0
    else:
        pull = Vector((0, 0, 1))
    return pull * strength * t


def grow(rng, mode, params):
    """Recursively grow a branch skeleton, returns a flat list of Segment."""
    segments = []
    height = params["height"]
    levels = params["branch_levels"]

    def spawn(origin, direction, length, r0, r1, depth, branch_index_in_parent):
        direction = direction.normalized()
        n_sub = max(1, params["segment_kinks"])
        pos = origin.copy()
        cur_dir = direction.copy()
        step = length / n_sub
        parent_seg = None
        for s in range(n_sub):
            kink = Vector((
                rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)
            )) * params["gnarl"] * (0.3 + 0.7 * depth / max(1, levels))
            cur_dir = (cur_dir + kink * 0.35).normalized()
            drop = Vector((0, 0, -1)) * params["gravity_droop"] * (depth ** 1.3) * 0.05
            cur_dir = (cur_dir + drop).normalized()
            bias = canopy_bias(pos, height, params["canopy_shape"], params["canopy_strength"], depth, levels)
            cur_dir = (cur_dir + bias).normalized()

            frac0 = s / n_sub
            frac1 = (s + 1) / n_sub
            seg_r0 = r0 + (r1 - r0) * frac0
            seg_r1 = r0 + (r1 - r0) * frac1
            new_pos = pos + cur_dir * step
            seg = Segment(pos.copy(), new_pos.copy(), seg_r0, seg_r1, depth, parent_seg)
            segments.append(seg)
            parent_seg = seg
            pos = new_pos

        if depth >= levels:
            return

        n_children = max(1, params["branch_count"] - (0 if depth == 0 else rng.randint(0, 1)))
        golden_angle = math.pi * (3.0 - math.sqrt(5.0))
        for c in range(n_children):
            if depth > 0 and rng.random() > params["branch_survival"]:
                continue
            twist_angle = c * golden_angle + rng.uniform(-0.3, 0.3)
            spread = math.radians(params["branch_angle"] + rng.uniform(-1, 1) * params["angle_random"])
            # build a direction offset from parent by `spread`, rotated
            # around parent axis by twist_angle
            up = Vector((0, 0, 1)) if abs(cur_dir.z) < 0.99 else Vector((1, 0, 0))
            side = cur_dir.cross(up).normalized()
            other = cur_dir.cross(side).normalized()
            offset = (side * math.cos(twist_angle) + other * math.sin(twist_angle)) * math.sin(spread)
            child_dir = (cur_dir * math.cos(spread) + offset).normalized()

            child_len = length * params["length_falloff"] * rng.uniform(0.75, 1.15)
            child_r0 = seg_r1 * params["radius_falloff"]
            child_r1 = child_r0 * 0.3
            if child_len < params["min_length"]:
                continue
            spawn(pos, child_dir, child_len, child_r0, child_r1, depth + 1, c)

    if mode == "TREE":
        n_stems = 1
    else:
        n_stems = params["base_stems"]

    for i in range(n_stems):
        base_dir = Vector((0, 0, 1))
        if mode == "BUSH":
            jitter = math.radians(params["base_spread"])
            base_dir = Vector((
                math.sin(jitter) * math.cos(i * 2.399 + rng.uniform(-0.4, 0.4)),
                math.sin(jitter) * math.sin(i * 2.399 + rng.uniform(-0.4, 0.4)),
                math.cos(jitter),
            )).normalized()
        origin = Vector((
            rng.uniform(-0.05, 0.05) * height if mode == "BUSH" else 0.0,
            rng.uniform(-0.05, 0.05) * height if mode == "BUSH" else 0.0,
            0.0,
        ))
        stem_len = height / n_stems ** 0.15 * rng.uniform(0.9, 1.1)
        spawn(origin, base_dir, stem_len, params["trunk_radius"], params["trunk_radius"] * params["radius_falloff"], 0, i)

    return segments


# ---------------------------------------------------------------------------
# mesh building
# ---------------------------------------------------------------------------

def build_trunk_mesh(segments, sides=6):
    bm = bmesh.new()
    ring_cache = {}

    def make_ring(center, direction, radius):
        direction = direction.normalized()
        up = Vector((0, 0, 1)) if abs(direction.z) < 0.99 else Vector((1, 0, 0))
        side = direction.cross(up).normalized()
        other = direction.cross(side).normalized()
        verts = []
        for i in range(sides):
            ang = 2 * math.pi * i / sides
            offset = side * math.cos(ang) * radius + other * math.sin(ang) * radius
            verts.append(bm.verts.new(center + offset))
        return verts

    for seg in segments:
        direction = seg.end - seg.start
        if direction.length < 1e-6:
            continue
        key0 = id(seg.parent) if seg.parent else None
        if key0 is not None and key0 in ring_cache:
            ring0 = ring_cache[key0]
        else:
            ring0 = make_ring(seg.start, direction, seg.r0)
        ring1 = make_ring(seg.end, direction, seg.r1)
        ring_cache[id(seg)] = ring1
        for i in range(sides):
            j = (i + 1) % sides
            try:
                bm.faces.new((ring0[i], ring0[j], ring1[j], ring1[i]))
            except ValueError:
                pass
        if not any(s.parent is seg for s in segments):
            try:
                bm.faces.new(ring1)
            except ValueError:
                pass

    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bm.normal_update()
    mesh = bpy.data.meshes.new("FoliageTrunk")
    bm.to_mesh(mesh)
    bm.free()
    return mesh


def pick_card(rng, atlas, kind_weights):
    kinds = [k for k in kind_weights if atlas["by_kind"].get(k)]
    if not kinds:
        return None
    weights = [kind_weights[k] for k in kinds]
    kind = rng.choices(kinds, weights=weights, k=1)[0]
    return rng.choice(atlas["by_kind"][kind])


def build_card_mesh(segments, rng, atlas, params):
    bm = bmesh.new()
    color_layer = bm.loops.layers.color.new("tint")

    kind_weights = {
        "leaf": params["mix_leaf"],
        "cluster": params["mix_cluster"],
        "branch": params["mix_branch"],
    }
    height = params["height"]
    eligible = [s for s in segments if s.depth >= params["foliage_start_depth"]]
    if not eligible:
        eligible = segments

    for seg in eligible:
        seg_len = (seg.end - seg.start).length
        n_cards = max(0, int(rng.gauss(params["foliage_density"] * seg_len * 10, 0.5)))
        direction = (seg.end - seg.start).normalized() if seg_len > 1e-6 else Vector((0, 0, 1))
        for _ in range(n_cards):
            card = pick_card(rng, atlas, kind_weights)
            if card is None:
                continue
            t = rng.random()
            pos = seg.start.lerp(seg.end, t)
            radius = seg.r0 + (seg.r1 - seg.r0) * t

            up = Vector((0, 0, 1)) if abs(direction.z) < 0.99 else Vector((1, 0, 0))
            side = direction.cross(up).normalized()
            other = direction.cross(side).normalized()
            ang = rng.uniform(0, 2 * math.pi)
            outward = (side * math.cos(ang) + other * math.sin(ang)).normalized()

            phototropism = Vector((0, 0, 1)) * params["phototropism"]
            normal = (outward * (1.0 - params["phototropism"]) + phototropism).normalized()
            tilt = math.radians(rng.uniform(-params["tilt_variance"], params["tilt_variance"]))
            tilt_axis = normal.cross(Vector((0, 0, 1))).normalized() if abs(normal.z) < 0.999 else side
            normal = Matrix.Rotation(tilt, 4, tilt_axis) @ normal

            pos = pos + outward * radius * 0.6

            aspect = card.get("aspect", 1.0)
            size = params["card_size"] * rng.uniform(1.0 - params["card_size_variance"], 1.0 + params["card_size_variance"])
            w = size
            h = size / max(0.2, aspect)

            tangent = normal.cross(Vector((0, 0, 1)))
            if tangent.length < 1e-4:
                tangent = normal.cross(Vector((1, 0, 0)))
            tangent = tangent.normalized()
            bitangent = normal.cross(tangent).normalized()
            roll = rng.uniform(0, 2 * math.pi)
            rot = Matrix.Rotation(roll, 4, normal)
            tangent = (rot @ tangent.to_4d()).to_3d()
            bitangent = (rot @ bitangent.to_4d()).to_3d()

            droop = params["card_droop"] * (0.3 + 0.7 * (1.0 - seg.depth / max(1, params["branch_levels"])))
            pos = pos - Vector((0, 0, 1)) * droop * h * 0.3

            v0 = pos - tangent * w * 0.5 - bitangent * h * 0.5
            v1 = pos + tangent * w * 0.5 - bitangent * h * 0.5
            v2 = pos + tangent * w * 0.5 + bitangent * h * 0.5
            v3 = pos - tangent * w * 0.5 + bitangent * h * 0.5
            bv = [bm.verts.new(v) for v in (v0, v1, v2, v3)]
            face = bm.faces.new(bv)

            u0, v0uv, u1, v1uv = card["uv"]
            uv_layer = bm.loops.layers.uv.verify()
            uvs = [(u0, v0uv), (u1, v0uv), (u1, v1uv), (u0, v1uv)]
            for loop, uv in zip(face.loops, uvs):
                loop[uv_layer].uv = uv

            hue_shift = rng.uniform(-1, 1) * params["color_variance"]
            val_shift = 1.0 + rng.uniform(-1, 1) * params["color_variance"] * 0.5
            tint = (
                max(0.0, min(2.0, val_shift + hue_shift * 0.15)),
                max(0.0, min(2.0, val_shift - hue_shift * 0.05)),
                max(0.0, min(2.0, val_shift - hue_shift * 0.15)),
                1.0,
            )
            for loop in face.loops:
                loop[color_layer] = tint

    bm.normal_update()
    mesh = bpy.data.meshes.new("FoliageCards")
    bm.to_mesh(mesh)
    bm.free()
    return mesh


# ---------------------------------------------------------------------------
# materials
# ---------------------------------------------------------------------------

def get_or_create_bark_material():
    name = "Foliage_Bark_Procedural"
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    tex_coord = nt.nodes.new("ShaderNodeTexCoord")
    noise1 = nt.nodes.new("ShaderNodeTexNoise")
    noise1.inputs["Scale"].default_value = 18.0
    noise1.inputs["Detail"].default_value = 6.0
    wave = nt.nodes.new("ShaderNodeTexWave")
    wave.wave_type = 'BANDS'
    wave.inputs["Scale"].default_value = 6.0
    wave.inputs["Distortion"].default_value = 3.0
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (0.045, 0.028, 0.018, 1.0)
    ramp.color_ramp.elements[1].color = (0.13, 0.08, 0.05, 1.0)
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.35

    nt.links.new(tex_coord.outputs["Object"], noise1.inputs["Vector"])
    nt.links.new(tex_coord.outputs["Object"], wave.inputs["Vector"])
    nt.links.new(wave.outputs["Color"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(noise1.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    bsdf.inputs["Roughness"].default_value = 0.9
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


def get_or_create_card_material(atlas_png_path: str, translucency: float, roughness: float):
    name = f"Foliage_Cards_{Path(atlas_png_path).stem}"
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    mat.blend_method = 'HASHED'
    if hasattr(mat, "shadow_method"):
        mat.shadow_method = 'HASHED'
    nt = mat.node_tree
    nt.nodes.clear()

    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    translu = nt.nodes.new("ShaderNodeBsdfTranslucent")
    mix = nt.nodes.new("ShaderNodeMixShader")
    tex = nt.nodes.new("ShaderNodeTexImage")
    img = bpy.data.images.get(Path(atlas_png_path).name)
    if img is None:
        img = bpy.data.images.load(atlas_png_path)
    tex.image = img
    attr = nt.nodes.new("ShaderNodeAttribute")
    attr.attribute_name = "tint"
    hsv = nt.nodes.new("ShaderNodeMixRGB")
    hsv.blend_type = 'MULTIPLY'
    hsv.inputs["Fac"].default_value = 1.0

    bsdf.inputs["Roughness"].default_value = roughness
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.2

    nt.links.new(tex.outputs["Color"], hsv.inputs["Color1"])
    nt.links.new(attr.outputs["Color"], hsv.inputs["Color2"])
    nt.links.new(hsv.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
    nt.links.new(hsv.outputs["Color"], translu.inputs["Color"])
    nt.links.new(bsdf.outputs["BSDF"], mix.inputs[1])
    nt.links.new(translu.outputs["BSDF"], mix.inputs[2])
    mix.inputs["Fac"].default_value = translucency
    nt.links.new(mix.outputs["Shader"], out.inputs["Surface"])
    return mat


# ---------------------------------------------------------------------------
# operator
# ---------------------------------------------------------------------------

class MESH_OT_generate_foliage(bpy.types.Operator):
    """Generate a bush or tree built from foliage card cutouts"""
    bl_idname = "mesh.generate_foliage"
    bl_label = "Generate Foliage"
    bl_options = {'REGISTER', 'UNDO'}

    atlas_json: StringProperty(
        name="Atlas JSON", subtype='FILE_PATH',
        description="Output of prep_foliage_atlas.py (<name>_atlas.json)")

    mode: EnumProperty(
        name="Type", items=[('BUSH', "Bush", ""), ('TREE', "Tree", "")], default='TREE')
    seed: IntProperty(name="Seed", default=1, min=0)

    height: FloatProperty(name="Height", default=3.0, min=0.1, soft_max=15.0)
    trunk_radius: FloatProperty(name="Trunk Radius", default=0.08, min=0.002, soft_max=0.5)
    branch_levels: IntProperty(name="Branch Levels", default=4, min=1, max=7)
    branch_count: IntProperty(name="Branches / Node", default=3, min=1, max=6)
    branch_survival: FloatProperty(name="Branch Survival", default=0.85, min=0.1, max=1.0,
                                    description="Chance each candidate child branch actually grows")
    branch_angle: FloatProperty(name="Branch Angle", default=35.0, min=0.0, max=90.0, subtype='UNSIGNED')
    angle_random: FloatProperty(name="Angle Randomness", default=12.0, min=0.0, max=45.0)
    length_falloff: FloatProperty(name="Length Falloff", default=0.68, min=0.2, max=0.95)
    radius_falloff: FloatProperty(name="Radius Falloff", default=0.65, min=0.2, max=0.95)
    min_length: FloatProperty(name="Min Branch Length", default=0.05, min=0.005, soft_max=1.0)
    segment_kinks: IntProperty(name="Kinks / Branch", default=3, min=1, max=8,
                                description="Sub-segments per branch, for organic curvature")
    gnarl: FloatProperty(name="Gnarl", default=0.18, min=0.0, max=1.0,
                          description="Random per-segment kink strength")
    gravity_droop: FloatProperty(name="Gravity Droop", default=0.15, min=0.0, max=1.0)

    canopy_shape: EnumProperty(name="Canopy Shape", items=[
        ('SPHERE', "Round", ""), ('CONE', "Conical", ""), ('VASE', "Vase", "")], default='SPHERE')
    canopy_strength: FloatProperty(name="Canopy Pull", default=0.5, min=0.0, max=1.0)

    base_stems: IntProperty(name="Base Stems (Bush)", default=4, min=1, max=12)
    base_spread: FloatProperty(name="Base Spread (Bush)", default=35.0, min=0.0, max=80.0)

    foliage_start_depth: IntProperty(name="Foliage Start Depth", default=1, min=0, max=6,
                                      description="Branches shallower than this stay bare")
    foliage_density: FloatProperty(name="Foliage Density", default=0.35, min=0.0, soft_max=2.0)
    card_size: FloatProperty(name="Card Size", default=0.18, min=0.005, soft_max=1.0)
    card_size_variance: FloatProperty(name="Card Size Variance", default=0.35, min=0.0, max=0.95)
    card_droop: FloatProperty(name="Card Droop", default=0.2, min=0.0, max=1.0)
    tilt_variance: FloatProperty(name="Card Tilt Variance", default=35.0, min=0.0, max=90.0)
    phototropism: FloatProperty(name="Upward Bias", default=0.25, min=0.0, max=1.0)

    mix_leaf: FloatProperty(name="Mix: Single Leaf", default=1.0, min=0.0, max=5.0)
    mix_cluster: FloatProperty(name="Mix: Cluster", default=1.5, min=0.0, max=5.0)
    mix_branch: FloatProperty(name="Mix: Branch Card", default=0.8, min=0.0, max=5.0)

    color_variance: FloatProperty(name="Color Variance", default=0.25, min=0.0, max=1.0)
    translucency: FloatProperty(name="Leaf Translucency", default=0.35, min=0.0, max=1.0)
    roughness: FloatProperty(name="Leaf Roughness", default=0.45, min=0.0, max=1.0)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "atlas_json")
        layout.prop(self, "mode")
        layout.prop(self, "seed")
        box = layout.box()
        box.label(text="Structure")
        box.prop(self, "height")
        box.prop(self, "trunk_radius")
        box.prop(self, "branch_levels")
        box.prop(self, "branch_count")
        box.prop(self, "branch_survival")
        box.prop(self, "branch_angle")
        box.prop(self, "angle_random")
        box.prop(self, "length_falloff")
        box.prop(self, "radius_falloff")
        box.prop(self, "min_length")
        box.prop(self, "segment_kinks")
        box.prop(self, "gnarl")
        box.prop(self, "gravity_droop")
        box = layout.box()
        box.label(text="Canopy")
        box.prop(self, "canopy_shape")
        box.prop(self, "canopy_strength")
        if self.mode == 'BUSH':
            box.prop(self, "base_stems")
            box.prop(self, "base_spread")
        box = layout.box()
        box.label(text="Foliage Cards")
        box.prop(self, "foliage_start_depth")
        box.prop(self, "foliage_density")
        box.prop(self, "card_size")
        box.prop(self, "card_size_variance")
        box.prop(self, "card_droop")
        box.prop(self, "tilt_variance")
        box.prop(self, "phototropism")
        box.prop(self, "mix_leaf")
        box.prop(self, "mix_cluster")
        box.prop(self, "mix_branch")
        box = layout.box()
        box.label(text="Material")
        box.prop(self, "color_variance")
        box.prop(self, "translucency")
        box.prop(self, "roughness")

    def execute(self, context):
        atlas = load_atlas(bpy.path.abspath(self.atlas_json))
        if atlas is None:
            self.report({'ERROR'}, "Set a valid Atlas JSON path (output of prep_foliage_atlas.py)")
            return {'CANCELLED'}

        rng = random.Random(self.seed)
        params = {k: getattr(self, k) for k in (
            "height", "trunk_radius", "branch_levels", "branch_count", "branch_survival",
            "branch_angle", "angle_random", "length_falloff", "radius_falloff", "min_length",
            "segment_kinks", "gnarl", "gravity_droop", "canopy_shape", "canopy_strength",
            "base_stems", "base_spread", "foliage_start_depth", "foliage_density",
            "card_size", "card_size_variance", "card_droop", "tilt_variance", "phototropism",
            "mix_leaf", "mix_cluster", "mix_branch", "color_variance",
        )}

        segments = grow(rng, self.mode, params)
        if not segments:
            self.report({'ERROR'}, "No branches generated - check parameters")
            return {'CANCELLED'}

        trunk_mesh = build_trunk_mesh(segments)
        card_mesh = build_card_mesh(segments, rng, atlas, params)

        atlas_png = str(Path(atlas["json_dir"]) / atlas["atlas"])
        bark_mat = get_or_create_bark_material()
        card_mat = get_or_create_card_material(atlas_png, self.translucency, self.roughness)

        trunk_mesh.materials.append(bark_mat)
        card_mesh.materials.append(card_mat)

        base_name = f"Foliage_{self.mode.title()}"
        root = bpy.data.objects.new(base_name, None)
        context.collection.objects.link(root)

        trunk_obj = bpy.data.objects.new(f"{base_name}_Trunk", trunk_mesh)
        trunk_obj.parent = root
        context.collection.objects.link(trunk_obj)
        for poly in trunk_obj.data.polygons:
            poly.use_smooth = True

        card_obj = bpy.data.objects.new(f"{base_name}_Foliage", card_mesh)
        card_obj.parent = root
        context.collection.objects.link(card_obj)

        for o in (trunk_obj, card_obj):
            o.select_set(True)
        context.view_layer.objects.active = root

        self.report({'INFO'}, f"Foliage: {len(segments)} branch segments, {len(card_mesh.polygons)} cards")
        return {'FINISHED'}


def menu_func(self, context):
    self.layout.operator(MESH_OT_generate_foliage.bl_idname, text="Foliage (Cards)", icon='OUTLINER_OB_FORCE_FIELD')


# ---------------------------------------------------------------------------
# sidebar panel (convenience - sets last-atlas default + quick presets)
# ---------------------------------------------------------------------------

class VIEW3D_PT_foliage(bpy.types.Panel):
    bl_label = "Foliage Generator"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Foliage"

    def draw(self, context):
        layout = self.layout
        layout.label(text="Add > Mesh > Foliage (Cards)")
        layout.label(text="then tweak sliders in the")
        layout.label(text="Adjust Last Operation panel.")
        layout.separator()
        col = layout.column(align=True)
        op = col.operator(MESH_OT_generate_foliage.bl_idname, text="Quick Tree")
        op.mode = 'TREE'
        op2 = col.operator(MESH_OT_generate_foliage.bl_idname, text="Quick Bush")
        op2.mode = 'BUSH'


classes = (MESH_OT_generate_foliage, VIEW3D_PT_foliage)


def register():
    for c in classes:
        bpy.utils.register_class(c)
    bpy.types.VIEW3D_MT_mesh_add.append(menu_func)


def unregister():
    bpy.types.VIEW3D_MT_mesh_add.remove(menu_func)
    for c in reversed(classes):
        bpy.utils.unregister_class(c)


if __name__ == "__main__":
    register()
