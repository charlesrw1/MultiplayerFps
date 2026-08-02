"""
Cuts a hand-painted foliage reference sheet (leaves/clusters/twigs/acorns
scattered on a blurry green/brown backdrop, no alpha) into a clean RGBA
atlas + a JSON card index that the Blender foliage generator consumes.

Usage (run with `py`, not blender):
    py Scripts/blender_foliage/prep_foliage_atlas.py <input.png> [output_dir]

Outputs into output_dir (default: alongside input):
    <name>_atlas.png    - same layout as input, background made transparent
    <name>_atlas.json   - per-card bounding boxes + uv rects + kind
    <name>_debug.png    - visualization of detected cards (for tuning)

Approach:
  The backdrop is a smooth low-frequency gradient; every painted element
  (leaf veins, twig bark, acorn caps) has high-frequency detail that a
  strong gaussian blur washes out. So: pixels where original ~= blurred
  are "smooth"; smooth regions connected to the image border are
  background. Everything else is foreground. Foreground is then split
  into connected components and each is classified into a card "kind"
  by color + shape heuristics.
"""
import sys
import json
from pathlib import Path

import numpy as np
from PIL import Image
from scipy.ndimage import (
    gaussian_filter, label, find_objects, binary_closing, binary_dilation,
    binary_erosion, binary_fill_holes, distance_transform_edt,
)

SIGMA = 5.0
SMOOTH_THRESH = 0.02
MIN_CARD_AREA = 40
FEATHER_PX = 1.5


def build_alpha(rgb: np.ndarray) -> np.ndarray:
    blurred = gaussian_filter(rgb, sigma=(SIGMA, SIGMA, 0))
    diff = np.abs(rgb - blurred).sum(axis=2)
    detail = diff >= SMOOTH_THRESH  # high-frequency = painted element (edges, veins, bark)

    # bridge small gaps in outlines so interiors fully enclose, then fill
    # them in (a leaf's flat-color interior reads as "smooth" too, but it's
    # surrounded by its own detailed edge, so filling closes it back up).
    # smooth regions that are NOT enclosed by any edge are true background
    # and stay empty.
    detail_closed = binary_closing(detail, structure=np.ones((3, 3)), iterations=1)
    filled = binary_fill_holes(detail_closed)
    foreground = binary_erosion(filled, structure=np.ones((3, 3)), iterations=1)

    # feather edge with a signed distance transform for soft AA alpha
    dist_out = distance_transform_edt(~foreground)
    dist_in = distance_transform_edt(foreground)
    signed = np.where(foreground, dist_in, -dist_out)
    alpha = np.clip(signed / FEATHER_PX + 0.5, 0.0, 1.0)
    return alpha, foreground


def classify(rgb_crop: np.ndarray, mask_crop: np.ndarray, w: int, h: int) -> str:
    px = rgb_crop[mask_crop]
    if len(px) == 0:
        return "leaf"
    r, g, b = px[:, 0].mean(), px[:, 1].mean(), px[:, 2].mean()
    area = mask_crop.sum()
    long_side, short_side = max(w, h), max(1, min(w, h))
    aspect = long_side / short_side
    is_green = g > r and g > b * 0.9
    is_brown = r >= g >= b or (r > g and r > b)

    if is_brown and aspect > 4.0 and area < 3000:
        return "twig"
    if is_brown and aspect < 2.2 and area < 4000:
        return "acorn"
    if is_green and aspect > 2.3 and area < 6000:
        return "leaf"
    if long_side > 260 or (is_green and is_brown is False and area > 9000):
        return "branch"
    if is_green:
        return "cluster"
    return "branch"


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    src_path = Path(sys.argv[1])
    out_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else src_path.parent
    out_dir.mkdir(parents=True, exist_ok=True)
    stem = src_path.stem

    img = Image.open(src_path).convert("RGB")
    rgb = np.asarray(img).astype(np.float32) / 255.0
    h_full, w_full = rgb.shape[:2]

    alpha, foreground = build_alpha(rgb)

    rgba = np.dstack([rgb, alpha]).astype(np.float32)
    rgba_u8 = np.clip(rgba * 255.0, 0, 255).astype(np.uint8)
    atlas_path = out_dir / f"{stem}_atlas.png"
    Image.fromarray(rgba_u8, mode="RGBA").save(atlas_path)

    # separate step for splitting into components: erode a bit further so
    # thin background gaps between neighboring elements reliably cut them
    # apart, without shrinking the actual alpha/edge quality above.
    card_mask = binary_erosion(foreground, structure=np.ones((3, 3)), iterations=2)
    labels, _ = label(card_mask)
    objs = find_objects(labels)

    cards = []
    debug = np.asarray(img).copy()
    cid = 0
    margin = 4
    for i, sl in enumerate(objs):
        if sl is None:
            continue
        # recover the full (un-eroded) silhouette of this component within
        # a padded window, then tighten the bbox back to its real extent.
        y0p, y1p = max(0, sl[0].start - margin), min(h_full, sl[0].stop + margin)
        x0p, x1p = max(0, sl[1].start - margin), min(w_full, sl[1].stop + margin)
        local = labels[y0p:y1p, x0p:x1p] == (i + 1)
        recovered = binary_dilation(local, iterations=margin) & foreground[y0p:y1p, x0p:x1p]
        ys, xs = np.where(recovered)
        if len(ys) == 0:
            continue
        y0, y1 = y0p + int(ys.min()), y0p + int(ys.max()) + 1
        x0, x1 = x0p + int(xs.min()), x0p + int(xs.max()) + 1
        w, h = x1 - x0, y1 - y0
        blob_mask = recovered[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
        area = int(blob_mask.sum())
        if area < MIN_CARD_AREA:
            continue
        kind = classify(rgb[y0:y1, x0:x1], blob_mask, w, h)
        u0, u1 = x0 / w_full, x1 / w_full
        v0, v1 = 1.0 - y1 / h_full, 1.0 - y0 / h_full
        cards.append({
            "id": cid,
            "kind": kind,
            "rect_px": [int(x0), int(y0), int(x1), int(y1)],
            "uv": [round(u0, 5), round(v0, 5), round(u1, 5), round(v1, 5)],
            "aspect": round(w / max(1, h), 3),
            "area_px": area,
        })
        cid += 1
        debug[y0:y1, x0:x0 + 1] = [255, 0, 0]
        debug[y0:y1, x1 - 1:x1] = [255, 0, 0]
        debug[y0:y0 + 1, x0:x1] = [255, 0, 0]
        debug[y1 - 1:y1, x0:x1] = [255, 0, 0]

    counts = {}
    for c in cards:
        counts[c["kind"]] = counts.get(c["kind"], 0) + 1

    json_path = out_dir / f"{stem}_atlas.json"
    json_path.write_text(json.dumps({
        "atlas": atlas_path.name,
        "width": w_full,
        "height": h_full,
        "cards": cards,
    }, indent=2))

    debug_path = out_dir / f"{stem}_debug.png"
    Image.fromarray(debug).save(debug_path)

    print(f"wrote {atlas_path}")
    print(f"wrote {json_path}  ({len(cards)} cards)  by kind: {counts}")
    print(f"wrote {debug_path}  (inspect this — tune SIGMA/SMOOTH_THRESH if cutout looks wrong)")


if __name__ == "__main__":
    main()
