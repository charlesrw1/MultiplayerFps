"""
Generate texture import settings (.tis) and material files for crack1 decal.

Outputs:
  Data/materials/decals/crack1/crack1_normal.tis
  Data/materials/decals/crack1/crack1_mask.tis
  Data/materials/decals/crack1/crack1_ao.tis
  Data/materials/decals/decal_normalmap_pom.mm   (master — normal-only decal with POM)
  Data/materials/decals/crack1_decal.mi           (instance for crack1 textures)

Usage:
  py Scripts/gen_crack_decal.py
"""

import json
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DATA_ROOT = REPO_ROOT / "Data"
CRACK1_DIR = DATA_ROOT / "materials" / "decals" / "crack1"
DECALS_DIR = DATA_ROOT / "materials" / "decals"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def write_tis(dst: Path, src_file: str, *, is_normalmap: bool, is_srgb: bool, resize_width: int = 1024):
    settings = {
        "__classname": "TextureImportSettings",
        "is_generated": False,
        "is_normalmap": is_normalmap,
        "is_srgb": is_srgb,
        "resize_width": resize_width,
        "src_file": src_file,
    }
    dst.write_text("!json\n" + json.dumps(settings, indent=1) + "\n", encoding="utf-8")
    print(f"  wrote {dst.relative_to(DATA_ROOT)}")


MASTER_MM = """\
TYPE MaterialMaster
DOMAIN Decal
OPT BlendMode Blend
OPT WriteNormal

VAR texture2D Normal    "_flat_normal"
VAR texture2D Mask      "_white"
VAR texture2D HeightMap "_black"
VAR float NormalStrength 1.0
VAR float PomScale       0.04
VAR float PomLayers      8.0

_FS_BEGIN

vec3 uncompress_normal(vec3 bc_normal)
{
    vec3 s = bc_normal * 2.0 - vec3(1.0);
    float x2 = s.x * s.x;
    float y2 = s.y * s.y;
    s.z = sqrt(max(1.0 - x2 - y2, 0.0));
    return s;
}

vec3 get_view_tangent()
{
    vec3 V = normalize(g.viewpos_time.xyz - FS_IN_FragPos);
    return FS_IN_TBN * V;
}

// Parallax occlusion mapping — returns refined UV.
vec2 do_parallax_mapping(vec2 uv, vec3 viewTS, float scale, float num_layers)
{
    float layer_depth  = 1.0 / num_layers;
    float cur_depth    = 0.0;
    // Move UVs in the opposite direction of the view projected onto the surface.
    vec2 delta = (viewTS.xy / max(abs(viewTS.z), 0.001)) * scale / num_layers;

    vec2 cur_uv = uv;
    float depth = 1.0 - texture(HeightMap, cur_uv).r;

    // Step until we pass below the height field.
    while (cur_depth < depth) {
        cur_uv   -= delta;
        depth     = 1.0 - texture(HeightMap, cur_uv).r;
        cur_depth += layer_depth;
    }

    // Binary-search refinement between the last two steps.
    vec2 prev_uv   = cur_uv + delta;
    float after    = depth - cur_depth;
    float before   = (1.0 - texture(HeightMap, prev_uv).r) - cur_depth + layer_depth;
    float weight   = after / (after - before);
    return mix(cur_uv, prev_uv, weight);
}

void FSmain()
{
    vec3 viewTS = get_view_tangent();

    // More layers when viewing at a glancing angle or up close.
    float n_layers = mix(PomLayers * 2.0, PomLayers,
                         abs(dot(vec3(0.0, 0.0, 1.0), viewTS)));

    vec2 uv = do_parallax_mapping(FS_IN_Texcoord, viewTS, PomScale, n_layers);

    // Opacity from mask (crack silhouette).
    OPACITY = texture(Mask, uv).r;

    // Normal decode + optional strength scale.
    vec3 n  = uncompress_normal(texture(Normal, uv).xyz);
    n.xy   *= NormalStrength;
    NORMALMAP = normalize(n);
}

_FS_END
"""

INSTANCE_MI = """\
TYPE MaterialInstance
PARENT materials/decals/decal_normalmap_pom.mm

VAR Normal      materials/decals/crack1/crack1_normal.dds
VAR Mask        materials/decals/crack1/crack1_mask.dds
# Use mask as height proxy: crack (dark) = deep depression.
VAR HeightMap   materials/decals/crack1/crack1_mask.dds
VAR NormalStrength  1.0
VAR PomScale        0.04
VAR PomLayers       8.0
"""

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print("=== gen_crack_decal.py ===\n")

    # 1. Texture import settings
    print("[1] Texture import settings (.tis):")
    write_tis(
        CRACK1_DIR / "crack1_normal.tis",
        src_file="crack1_normal.png",
        is_normalmap=True,
        is_srgb=False,
        resize_width=1024,
    )
    write_tis(
        CRACK1_DIR / "crack1_mask.tis",
        src_file="crack1_mask.png",
        is_normalmap=False,
        is_srgb=False,
        resize_width=1024,
    )
    write_tis(
        CRACK1_DIR / "crack1_ao.tis",
        src_file="crack1_ao.png",
        is_normalmap=False,
        is_srgb=False,
        resize_width=1024,
    )

    # 2. Master material
    print("\n[2] Master material:")
    mm_path = DECALS_DIR / "decal_normalmap_pom.mm"
    mm_path.write_text(MASTER_MM, encoding="utf-8")
    print(f"  wrote {mm_path.relative_to(DATA_ROOT)}")

    # 3. Instance material
    print("\n[3] Instance material:")
    mi_path = DECALS_DIR / "crack1_decal.mi"
    mi_path.write_text(INSTANCE_MI, encoding="utf-8")
    print(f"  wrote {mi_path.relative_to(DATA_ROOT)}")

    print("\nDone. Reimport textures in the editor for .tis to take effect.")


if __name__ == "__main__":
    main()
