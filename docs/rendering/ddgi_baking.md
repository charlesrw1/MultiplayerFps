# DDGI Baking Parameters

DDGI (Dynamic Diffuse Global Illumination) bakes irradiance + depth probes into a 2D atlas. Probes live inside one or more `GiVolumeComponent` boxes; runtime sampling is controlled by global biases in `RaytraceTest.cpp`.

## Per-volume (`GiVolumeComponent`)

Defined in Source/Game/Components/LightComponents.h.

- `priority` (int, default 0) — overlap resolution. Higher-priority volume wins where two volumes intersect. Use a small interior volume with higher priority to densify probes around a room inside a larger outer volume.
- `xz_density` (float, default 2.0) — probes per metre along world X and Z. Higher = more probes, more bake cost, smaller leak radius.
- `y_density` (float, default 2.0) — probes per metre along world Y.
- `override_relocate_dist` (bool) — if true, use this volume's relocate values instead of the global default.
- `relocate_max_dist` (float, default 0.2 m) — max distance a probe can shift from its grid cell to escape geometry. Probes inside walls trace outward up to this radius. Too small = probes stuck in geo (black/leaking). Too large = probes drift between cells and flicker.
- `relocate_normal_push` (float, default 0.2 m) — extra push along surface normal after relocation, biases probes away from the wall they relocated onto.

## Global sampling biases (`RaytraceTest.cpp`)

These live as file-static floats; tunable live via the DDGI debug menu. Stored in `DdgiGlobals` UBO each frame.

- `normal_bias` (default 0.2) — shading-point offset along the surface normal before sampling the probe grid. Prevents self-occlusion / dark seams on flat surfaces. Raise if you see dark bands along walls; lower if light leaks at thin geometry.
- `view_bias` (default 0.3) — shading-point offset along the view direction. Reduces leaks at silhouettes and grazing angles. Raise to kill view-dependent leaks; too high pulls light through walls.
- `bounces` (default 4) — number of probe-trace bounces per bake pass. Each bounce re-traces and re-gathers using previous-pass irradiance. Diminishing returns past ~4.
- `relocate_normal_dist` (default 0.2) — global default for the per-volume `relocate_normal_push` when the volume does not override.
- `irrad_mult` (default 1.0) — post-multiplier on sampled irradiance. Debug knob, not a true bake parameter.
- `indirect_boost` (default 1.0, on `DdgiTesting`) — multiplier applied during probe gather: `probe_irrad += albedo * sample_irradiance() * indirect_boost`. Compounds across bounces — values >1 brighten indirect light non-linearly.
- `max_relocate_dist` (default 1.0 m, on `DdgiTesting`) — global cap on probe relocation trace distance.

## Hard-coded bake constants

In `Source/Render/RT/RaytraceTest_internal.h`:

- `MAX_RAYS = 256` — rays per probe per bounce.
- `ddgiIRRADTILE`, `ddgiDEPTHTILE` — atlas tile sizes for irradiance + depth octahedral encodings.

## Tuning order

1. Set volume `xz_density` / `y_density` for the spatial detail needed.
2. Bake. If probes are dark inside walls, raise `relocate_max_dist`.
3. If walls show dark seams, raise `normal_bias`.
4. If light leaks through thin walls, raise `view_bias` or add a denser high-`priority` interior volume.
5. Adjust `bounces` last — more bounces = longer bake, brighter indirect.

## See also

- [[rendering/materials]]
- Source: Source/Render/RT/RaytraceTest.cpp, Source/Game/Components/LightComponents.h
