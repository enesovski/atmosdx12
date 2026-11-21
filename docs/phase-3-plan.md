# Phase 3 plan: lighting and shadows

**Status:** ✅ implemented.

**Verification** (RTX 4060 Laptop):

- `/W4` Debug and Release builds have no warnings.
- GPU-based validation runs clean with 37 lights.
- Release with 133 lights, 3 of them shadowed, at about 1540x845: 2.8 ms GPU.
- Sun cascades, colored local lights, the light-count heat map and window resize were checked on screen.
- Sponza has no `BLEND` materials, so the transparent pass is culled there; it has not been seen on screen yet.

**Deviations from this plan:**

- The sun stays in `SceneEnvironment` (azimuth, elevation, intensity) rather than becoming a directional light
  component. Only point and spot lights are entities.
- PCF is a 3x3 tent of hardware comparisons, not a wider Poisson kernel.
- One transparent PSO with culling off covers single- and double-sided materials.
- Shadow maps are not cached between frames, and there are no motion vectors yet (Phase 4 TAA needs them).
- `SceneRenderer` was added between `Renderer` (frame loop, GPU) and the passes, so the renderer stays a frame
  driver and what is drawn lives in one place.

**Goal:** replace the temporary forward lambert pass with physically based deferred lighting. That means many lights,
sun shadows, image-based lighting from the sky, and a sorted pass for transparent materials.

**Done when:**

- Sponza is lit by the sun with stable cascaded shadows and 100+ point and spot lights at interactive frame rates
  (under 8 ms on an RTX 4060 at 1080p).
- Diffuse and specular IBL from the sky change with the sun.
- `BLEND` materials render in a sorted transparent pass.
- Debug views show the GBuffer channels, shadow cascades and light-cluster heat map.
- GPU validation is clean, and resize works.

**Frame after Phase 3:**

```text
Shadow Cascades[4]   depth-write   Sun Shadow Map (array)       per-cascade draw lists
Local Shadows        depth-write   Shadow Atlas                 spot (1 tile) / point (6 tiles)
Depth Prepass        depth-write   Scene Depth
GBuffer              depth-read(EQUAL)  GBufferA..C, Scene Color (emissive)
Light Clustering     compute       Cluster Light Lists (buffer)
IBL Update           compute       Sky Cubemap, Irradiance SH, Prefiltered Specular   only when the sun changes
Deferred Lighting    compute       reads GBuffer + depth + shadows + clusters + IBL, adds to Scene Color
Sky                  depth-read    Scene Color
Transparent          depth-read    Scene Color (alpha blend, sorted back to front, forward + clusters)
Tonemap / Overlay    as today
```

---

## Steps

Each step builds, runs, and keeps validation clean.

### S1. Engine prerequisites

- **Render graph:**
  - transient buffers (`RGBuffer`)
  - compute passes (`ReadWriteBuffer`, `Dispatch`)
  - texture arrays and cube maps (`TextureDesc::arraySize`, `dimension`)
  - per-slice RTV/DSV/UAV views, so one cascade or cube face can be bound
- **`GpuScene`, multi-view:**
  - `BuildDrawList(const Frustum&)` returns a per-view `DrawList` (buckets plus DrawData range).
  - The main view, each cascade and each local-light view cull independently.
- **View data:**
  - `ViewConstants` stays the camera.
  - Shadow views live in a per-frame `StructuredBuffer<ViewData>`.
  - Mesh depth shaders read their view through a root-constant `viewIndex`, so the same PSO serves every shadow
    view.

### S2. Lights in the scene

- **Components:**
  - `DirectionalLightComponent` (the sun; it replaces `SceneEnvironment`'s sun fields)
  - `PointLightComponent` and `SpotLightComponent`: color, intensity in lumens or candela, range, cone angles,
    casts shadows
- **Plumbing:** JSON serialization, `RenderScene::lights` filled by `RenderSceneBuilder`, and an ImGui light list
  with editing.
- **Scene:** `scenes/sponza.json` gains about 100 point lights placed along the arcades, plus a "spawn lights" debug
  button.

### S3. GBuffer pass (replaces `OpaquePass`)

| Target | Format | Contents |
|---|---|---|
| GBufferA | `R8G8B8A8_UNORM_SRGB` | base color rgb, material AO |
| GBufferB | `R10G10B10A2_UNORM` | octahedral normal (rg), unused (b), shading-model bits (a) |
| GBufferC | `R8G8B8A8_UNORM` | metallic, roughness, specular, unused |
| Scene Color | `R16G16B16A16_FLOAT` | emissive written directly (lighting adds on top) |

- Same buckets and `EQUAL` depth as today. Normal mapping moves here.

### S4. Clustered light culling (compute)

- **Clusters:** 16×9×24 froxels, with depth slices logarithmic in view Z (Olsson 2012, Doom 2016).
- **Light data:** one per-frame `StructuredBuffer<LightData>` for all lights.
- **Passes:**
  1. Cluster AABBs, computed once per resize or projection change.
  2. Light assignment: one thread group per cluster, sphere/cone vs AABB.
  3. Compaction into `{offset, count}` per cluster plus a flat index list.

### S5. Deferred lighting (compute)

- **Dispatch:** one thread per pixel, 8×8 groups.
- **Inputs:** depth reconstructed to world position, the GBuffer, and the pixel's cluster.
- **BRDF:** Cook-Torrance GGX (Karis 2013) with height-correlated Smith visibility, Schlick Fresnel, and Lambert
  diffuse scaled by (1 − F).
- **Lights:** sun with cascaded shadow lookup; clustered point and spot lights with shadow-atlas lookup; IBL
  ambient (S7).
- **Output:** added into Scene Color (UAV); the sky pass then fills the empty pixels.

### S6. Shadows

**Sun: 4 cascaded shadow maps, 2048² each, in a `Texture2DArray`.**

- Practical split scheme (log/linear blend λ = 0.75).
- Stable cascades: bounding-sphere fit and texel snapping, so shadows do not shimmer.
- Reverse-Z with depth clamp off and a pancaked near plane.
- 16-tap Poisson PCF with receiver-plane depth bias and normal-offset bias. PCSS-lite (blocker search) is an option.
- Cascade blend band and a debug color view.

**Local lights: one 8192² D32 shadow atlas.**

- Spot lights get 1 tile; point lights get 6 cube-face tiles.
- Tile size is chosen by screen coverage.
- Static lights are cached until something moves (a dirty flag per light).

### S7. Image-based lighting

When the sun changes, these run on compute:

1. Render the procedural sky into a 128² cube map.
2. Project it to 3rd-order spherical harmonics for diffuse irradiance.
3. Prefilter specular into 6 mips with GGX importance sampling (split-sum, Karis 2013).

A 2D BRDF LUT (scale, bias) is computed once at startup. Lighting adds `diffuse × SH + specular × (F₀ · A + B) ×
prefiltered`, both scaled by material AO.

### S8. Transparent pass

- **Ordering:** `BLEND` materials get their own bucket, sorted back to front by instance-bounds center.
- **Pipeline:** forward shading with the same BRDF, clustered lights, sun shadows and IBL. Alpha blend, depth
  read-only, after the sky.
- **Retire the workaround:** remove the "Blend renders as Mask" fallback.

### S9. Debug views and stats

- **View modes:**
  - lit
  - base color
  - normals
  - roughness / metallic
  - AO
  - shadow cascades
  - light-cluster heat map
- **Stats:** visible lights and the number of shadow-casting lights. Per-pass GPU timestamps arrive in Phase 5,
  but a simple whole-frame GPU timer is added now.

### S10. Verification and docs

- **Visual checks:**
  - Sponza at noon and at a low sun shows no acne, no peter-panning, and stable cascades while the camera moves.
  - 100+ lights hold the frame budget.
  - Toggling IBL shows the sky tint.
- **Debug layer:** GPU validation is clean.
- **Docs:** update ARCHITECTURE.md, CLAUDE.md and the roadmap, and record the decisions below.

---

## Decisions, alternatives, and who uses what

| Decision | Chosen | Alternatives (who uses them) | Why |
|---|---|---|---|
| Shading path | Deferred, compute lighting | Forward+ (Doom 2016, Godot 4, URP Forward+); visibility buffer (Nanite, The Forge) | Many lights at a cost independent of material count, easy screen-space effects next phase; matches Unreal and HDRP defaults. The visibility buffer is revisited with GPU-driven rendering (Phase 5). |
| GBuffer layout | 3 targets + emissive into scene color | Unreal: GBufferA–E + velocity; HDRP: 4 targets; thin GBuffers (Call of Duty) | Smallest layout that holds glTF PBR; emissive needs no extra target. |
| Normal encoding | Octahedral in 10:10 | Spheremap, raw xyz, 16-bit octahedral | Octahedral is uniform and cheap (Cigolle et al. 2014); 10 bits is enough for shading. |
| Light culling | Clustered (3D froxels) | Tiled 2D (Battlefield 3, Frostbite); per-light volumes (classic deferred) | Clusters handle depth discontinuities and serve the transparent forward pass too. |
| Lighting pass | Compute, one dispatch | Fullscreen pixel shader; stencil light volumes (classic) | One pass reads all lights per cluster; compute allows group-shared loads later. |
| Sun shadows | 4 stable CSM + PCF | Virtual shadow maps (Unreal 5); EVSM/MSM (filterable); ray-traced (Phase 7) | Proven, simple and fast; Unreal 5 VSM needs GPU-driven culling first. |
| Local shadows | Atlas with tiles sized by coverage | Per-light textures; cube maps | One resource and one PSO; HDRP and Frostbite use atlases. |
| Diffuse IBL | SH9 irradiance | Irradiance cube map | 9 coefficients, trivial to sample and to blend later with probes. |
| Specular IBL | Split-sum prefiltered cube + BRDF LUT | Real-time ray tracing; screen-space reflections only | Industry standard (Karis 2013, Unreal and Unity). |
| Motion vectors | Deferred to Phase 4 (camera reprojection from depth, plus velocity for moving objects) | Velocity in the GBuffer now | Nothing moves yet; TAA in Phase 4 defines the exact need. |
| Shadow views | One mesh depth PSO + `viewIndex` root constant | A PSO or constant buffer per view | The same draws serve camera, cascades and atlas tiles; prepares GPU-driven multi-view culling. |

## Risks

- **Shadow quality:** bias tuning across cascades. Expose bias sliders in the debug UI from day one.
- **Light count vs cluster memory:** cap lights per cluster (128) and log overflow.
- **IBL update cost:** a sun-slider drag recomputes IBL every frame. Rate-limit it to every N frames while dragging.
- **Unused render-graph aliasing:** shadow maps and GBuffers add about 200 MB. Aliasing in Phase 5 reclaims it;
  until then, watch `Pooled render targets` in the overlay.
