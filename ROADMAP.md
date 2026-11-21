# AtmosDX12 roadmap

This roadmap takes AtmosDX12 from a deferred renderer to a high-end real-time renderer with an editor like
Unity's or Unreal's. The focus is **rendering quality on static scenes**, with Sponza as the reference scene:

- image quality: temporal anti-aliasing, HDR post
- reflections, ambient occlusion and global illumination, each in a raster and a ray-traced form
- a path-traced reference to judge them against
- a physically based atmosphere

Gameplay-facing features (animation, particles, terrain) are out of scope for now.

Each phase ends with a "done when" check. Finish phases in order. Later phases assume that the earlier exit criteria
hold.

| Phase | Topic | Status |
|---|---|---|
| 0 | Foundation refactor | ✅ done |
| 1 | Render graph v1 and first geometry | ✅ done ([plan and decisions](docs/phase-1-plan.md)) |
| 2 | Scene, assets, and materials | ✅ done ([decisions](docs/phase-2-plan.md)) |
| 3 | Lighting and shadows (deferred + clustered) | ✅ done ([plan and decisions](docs/phase-3-plan.md)) |
| 4 | Editor UI: docking, hierarchy, inspector, render settings, view modes | ✅ done ([plan and decisions](docs/phase-4-plan.md)) |
| 5 | Image quality foundation: motion vectors, TAA, HDR post, material quality | |
| 6 | Screen-space lighting: GTAO, SSR, reflection probes, contact shadows | |
| 7 | Ray tracing: RT shadows, AO, reflections, reference path tracer | |
| 8 | Global illumination: DDGI probes, SSGI fallback | |
| 9 | Atmosphere and volumetrics | |
| 10 | Performance and scalability | |
| 11 | Optional: ReSTIR, virtual shadow maps, HDR output, more test scenes | |

---

## Do we need a render graph?

**Yes. It arrives in Phase 1**, together with the first geometry passes.

Unity (RenderGraph in URP/HDRP), Unreal (RDG), and Frostbite (FrameGraph) all use one. Once a frame has around ten
passes, a graph solves problems that hand-written code handles badly:

| Problem | Without a graph | With a graph |
|---|---|---|
| Resource barriers | Hand-written; they break whenever a pass is added, removed, or reordered | Derived from each pass's declared reads and writes |
| Transient textures (GBuffer, SSAO, bloom mips, shadow maps) | `Renderer` members that live forever and need resizing by hand | Described per frame; created, pooled, and later aliased automatically |
| Optional features (SSAO off, debug views) | `if` statements spread through `Render` | A pass whose output nobody reads is culled |
| Async compute, split barriers | Effectively impossible by hand | The graph knows the dependencies |
| Debugging the frame | Read the code | Dump or visualize the compiled graph |

It was not built in Phase 0 because two passes give nothing to design the API against. Phase 0 built what the graph
needs: textures with tracked state, a descriptor allocator, comparable texture descriptions, and resize. Phase 1 has
four real passes (depth, opaque, sky, tonemap), which is enough to shape the API correctly.

Render graph **v1** (Phase 1) covers:

- per-frame setup and execute callbacks
- transient textures pooled by description
- imported resources
- automatic transitions
- culling of unused passes
- declaration-order execution

Render graph **v2** (Phase 10) adds:

- memory aliasing through placed resources
- an async compute queue
- split or enhanced barriers

---

## Phase 0: foundation refactor ✅ done

The engine (`AtmosEngine` static library) and the app (`AtmosDX12` executable) are separate. The engine has:

- a bindless binding model (SM 6.6, one root signature)
- DXC
- descriptor heaps with free lists
- `Texture` with tracked state
- `CommandContext` with batched barriers
- a per-frame `UploadAllocator`
- `PipelineState` built from descriptor structs
- reverse-Z camera matrices
- window resize
- debug tooling (debug names, PIX markers, DRED, GPU validation flag, debug-layer messages printed to the console)

Verification: the output matches the pre-refactor build to within 1/255 per channel (GPU float rounding). Resize
works, and GPU-based validation reports nothing.

[ARCHITECTURE.md](ARCHITECTURE.md) describes the result and records each design decision with its alternatives.

---

## Phase 1: render graph v1 and first geometry ✅ done

**Verification:**

- The default scene and a test glTF (node hierarchy, rotation, scale, and an instance behind the camera that is
  culled) render correctly.
- With an empty scene, the sky is pixel-identical to Phase 0.
- A pass with an unused output is culled, and its texture is never allocated.
- Resize, minimize, and maximize are clean under `--gpu-validation`.
- Debug and Release build warning-free.

The full plan, with each decision's alternatives and the engines that use them, is in
[docs/phase-1-plan.md](docs/phase-1-plan.md).

The frame becomes:

1. Depth prepass
2. Opaque (temporary forward lambert + sun)
3. Sky (depth-tested at the far plane)
4. Tonemap

All four are recorded through the render graph. The phase also brings:

- glTF meshes with vertex pulling
- a fly camera
- CPU frustum culling
- the `Buffer` class, static uploads, and deferred resource release

**Done when:**

- a glTF scene (for example Sponza) renders correctly with the sky behind it
- `Renderer::Render` only builds a graph and contains no barrier code
- an unused pass is culled
- resize works, and GPU validation is clean

## Phase 2: scene, assets, and materials ✅ done

`scenes/sponza.json` streams Sponza in asynchronously, with all glTF texture slots, alpha-tested and double-sided
materials, and an ImGui overlay. See [docs/phase-2-plan.md](docs/phase-2-plan.md) for the decisions.

**Differences from the plan below:**

- **Scene model:** plain entity structs rather than EnTT.
- **Texture compression:** BC1/BC3/BC5 rather than BC7; CPU BC7 was too slow to cook.
- **Transparency:** `BLEND` materials render as masked until Phase 3's transparent pass.
- **Debug UI:** it is on by default (toggle with F1).
- **Stats:** per-pass stats show live/culled passes, draws and materials. Per-pass GPU timings come in Phase 4 (editor profiler).
- **Test coverage:** only Sponza was tested; there is no multi-model scene yet.

- **Scene model:** a gameplay-facing `Scene` with entities, a transform hierarchy, and components (mesh renderer,
  light, camera, environment), kept separate from the renderer's `RenderScene`. `RenderScene` holds flat arrays of
  render proxies. This is the Unreal FScene / Unity SRP split. Choose here between EnTT and plain entity structs.
- **Materials:** PBR metallic-roughness. Parameters live in a bindless GPU buffer, and textures are bindless indices.
  Opaque, alpha-tested, and transparent variants.
- **Textures:** DirectXTex loading, mip generation, BC7/BC5 compression with an offline cache, and correct sRGB
  versus linear handling.
- **Asset system:**
  - handle-based cache
  - asynchronous loading on a worker thread, using a copy queue
  - fallback assets
  - a cooked binary mesh format, so glTF is parsed only at import
- **Debug UI:** a Dear ImGui overlay, off by default, for render settings, stats, and the render graph dump.
- **Scene serialization:** JSON.

**Done when:** a scene file with many glTF models and materials loads asynchronously and renders with correct
textures, and the debug overlay shows per-pass stats.

## Phase 3: lighting and shadows ✅ done

Planned in detail, with each decision's alternatives: [docs/phase-3-plan.md](docs/phase-3-plan.md).

**Steps:**

1. **Engine prerequisites:**
   - render-graph buffers, compute passes, texture arrays and cube maps
   - multi-view `GpuScene` draw lists
   - per-view data for shadow views
2. **Light components:** directional, point and spot lights in the scene, JSON and UI; about 100 lights in Sponza.
3. **GBuffer pass:** 3 targets plus emissive into scene color; replaces `OpaquePass`.
4. **Clustered light culling:** 16×9×24 froxels, in compute.
5. **Deferred lighting:** GGX lighting in compute.
6. **Shadows:** cascaded shadow maps for the sun (4 × 2048², stable, PCF) and a shadow atlas for local lights.
7. **IBL:** sky cube map, SH9 irradiance, prefiltered specular, and a BRDF LUT.
8. **Transparent pass:** forward, sorted back to front.
9. **Debug views:** GBuffer channels, cascades and the cluster heat map; a GPU frame timer.

**Done when:** Sponza is lit by the sun with stable cascaded shadows and 100+ point and spot lights in under 8 ms at
1080p (RTX 4060), IBL follows the sun, and transparents render sorted.

## Phase 4: editor UI ✅ done

A Unity/Unreal-style editor shell: dockable, resizable panels, a consistent theme with proper fonts and icons, a
scene hierarchy with an inspector, a render-settings panel and viewport view modes. Every later phase adds its
settings and view modes here, so this comes before any new rendering feature.

Planned in detail, with layouts, settings and the style guide: [docs/phase-4-plan.md](docs/phase-4-plan.md).

**Steps:**

1. **Editor shell:** ImGui docking branch, an `editor/` library, theme, fonts (Inter, JetBrains Mono, icons), DPI
   scaling, main menu, default layout and layout persistence.
2. **Viewport panel:** the scene renders into an offscreen texture shown in a dockable panel, with a view-mode menu,
   camera menu, show flags and a stats overlay.
3. **Hierarchy and inspector:** selection, create/duplicate/delete/rename/reparent, property grid, transform, light,
   model and environment inspectors.
4. **Undo/redo** for every scene edit.
5. **Viewport tools:** transform gizmos, light icons, click selection, focus, selection outline.
6. **Render settings panel:** categories, quality presets, per-setting reset, saved with the scene.
7. **Tool panels:** profiler (per-pass GPU timings), render graph, console, content browser.

**Done when:** Sponza can be lit, edited and saved entirely in the editor (lights placed with gizmos, settings
tuned in panels), the layout survives restarts, and every render setting and view mode from Phase 3 is reachable
from the UI.

## Phase 5: image quality foundation

Temporal data and a proper HDR pipeline. Every later effect (SSR, ray-traced effects, GI) needs motion vectors,
history buffers and a denoise/accumulate step, and needs a believable final image to be judged by.

**Steps:**

1. **Iteration tools:** shader hot reload (file watcher → recompile → swap the affected PSOs) and screenshot capture
   with A/B comparison in the editor.
2. **Motion vectors:** camera reprojection from depth, plus a velocity pass for moving objects (previous transforms
   in `GpuScene`).
3. **TAA:**
   - Halton jitter in the projection, with a matching texture mip bias
   - history imported into the graph
   - YCoCg neighborhood clipping and velocity-weighted feedback
4. **Upscaling hook:** render at a lower internal resolution and resolve with TAA; FSR 2/3 behind the same interface.
5. **HDR post chain:**
   - histogram auto exposure with EV100 and exposure compensation
   - physically based bloom (downsample/upsample chain)
   - AgX and ACES tonemapping, and a color-grading LUT
6. **Material quality:**
   - specular anti-aliasing (normal-variance roughness, Kaplanyan 2016)
   - MikkTSpace tangents
   - BC7 base color through a GPU compressor
   - multiple-scattering energy compensation for GGX

**Done when:** a slow camera move through Sponza has no shimmering on edges or specular highlights, exposure adapts
between the sunlit atrium and the dark arcades, and bloom and tonemapping hold up on the sun and bright lights.

## Phase 6: screen-space lighting and reflection probes

The raster baseline for ambient occlusion and reflections. It is also the fallback when ray tracing is off or
unsupported.

**Steps:**

1. **GTAO** (Jimenez 2016):
   - half resolution with a bilateral upsample and temporal accumulation
   - bent normals
   - specular occlusion applied to IBL
2. **Hi-Z buffer:** a depth pyramid, shared later by SSR and GPU culling.
3. **Screen-space reflections:**
   - hierarchical tracing through the Hi-Z
   - roughness-aware ray reuse and a temporal filter
   - fades to the next fallback at screen edges and on misses
4. **Reflection probes:**
   - placed box probes (entities with a probe component), captured with the full renderer
   - box-projected parallax correction, blended by influence volume
   - order: SSR, then probes, then sky IBL
5. **Contact shadows:** a short screen-space ray march toward the sun and shadowed local lights, for detail the
   shadow maps miss.
6. **Lighting view modes:** AO only, reflections only, direct only, indirect only.

**Done when:** the atrium floor shows stable reflections of the columns and curtains, arcades and curtain folds are
occluded convincingly, and off-screen reflections fall back to probes without visible seams.

## Phase 7: ray tracing

DXR 1.1 with inline ray queries in compute, so ray-traced effects reuse the bindless vertex pulling and material
code already in place. Every effect has an on/off switch with the Phase 6 technique as its fallback.

**Steps:**

1. **Acceleration structures:**
   - a BLAS per mesh, built after upload (compacted)
   - a TLAS rebuilt per frame from the scene's instances
   - instance → geometry/material lookup for hit shading
   - alpha-tested geometry handled in the ray query
2. **Hit shading library:** fetch the hit's vertex attributes and material, then light it with the existing
   `SurfaceLighting` code (sun, clustered lights, sky).
3. **Ray-traced shadows:** soft sun shadows from a sun-disk cone and soft local-light shadows, with a denoiser.
   Cascades remain as the fallback.
4. **Ray-traced ambient occlusion** with a temporal and spatial denoiser.
5. **Ray-traced reflections:**
   - one ray per pixel at most, GGX-sampled, with hit lighting
   - a denoiser (SVGF/ReBLUR-style; NVIDIA NRD as an alternative)
   - SSR or probes for rough surfaces when budget-limited
6. **Reference path tracer:** a progressive, unbiased render mode in the viewport. It is the ground truth that every
   real-time effect is compared against in A/B screenshots.

**Done when:** RT shadows, AO and reflections each toggle against their raster fallbacks; the path tracer converges
on Sponza; and the real-time RT effects fit within 6 ms at 1080p on an RTX 4060 with temporally stable denoising.

## Phase 8: global illumination

Dynamic diffuse GI, so sunlight bouncing off the atrium floor lights the arcades and the curtains bleed color.

**Steps:**

1. **DDGI probe volumes** (Majercik 2019):
   - a probe grid fitted to the scene, updated with ray queries (rays per probe per frame)
   - octahedral irradiance and visibility (depth moments), with infinite bounces through the previous frame's probes
   - probe relocation and classification, to keep probes out of walls and turn off useless ones
2. **Apply GI** in deferred lighting and transparents. Sky visibility comes from the probes, so arcades stop
   receiving unoccluded sky light.
3. **Screen-space GI** as the non-RT fallback, and for small-scale detail on top of the probes.
4. **GI debug:** probe visualization, irradiance-only view, and a comparison against the path tracer.
5. **Emissive surfaces** contribute through the probe rays.

**Done when:** with only the sun, the arcades are lit by bounce light that matches the path-traced reference closely,
there are no visible light leaks through Sponza's walls, and the GI updates within a second when the sun moves.

## Phase 9: atmosphere and volumetrics

The project's namesake: a physically based sky that drives all outdoor lighting, and light shafts through the
atrium.

**Steps:**

1. **Atmosphere** (Hillaire 2020):
   - transmittance, multiple-scattering and sky-view LUTs
   - an aerial-perspective froxel volume
   - sun and sky color from transmittance, feeding IBL, probes and GI
2. **Volumetric fog** (Wronski 2014 / Hillaire 2015):
   - froxels lit by the sun shadow map (or RT shadows) and the clustered lights
   - temporal reprojection
   - local fog volumes
3. **Volumetric clouds** (optional): raymarched at quarter resolution with temporal reconstruction, and cloud
   shadows.

**Done when:** a sun sweep from -10° to 90° looks plausible from sunrise to twilight, with light shafts through the
Sponza atrium that stay stable in motion.

## Phase 10: performance and scalability

**Steps:**

1. **Render graph v2:**
   - transient memory aliasing with placed resources
   - an async compute queue, so RT, GI and fog overlap with shadows and the GBuffer
   - enhanced barriers
2. **Persistent GPU scene:** instance and material data resident on the GPU and updated by deltas, plus a unified
   geometry buffer.
3. **GPU culling:**
   - per-instance frustum culling
   - two-phase Hi-Z occlusion
   - `ExecuteIndirect` draws for every view (camera, cascades, atlas tiles)
4. **Parallel CPU:** a job system, parallel command recording per graph pass, and Tracy for CPU zones.
5. **Scalability:** quality presets mapped to every feature's settings, an offline shader compile step and an
   on-disk PSO cache for Release.

**Done when:** Sponza at the "High" preset with RT and GI runs under 16 ms at 1080p on an RTX 4060, async compute
overlap is visible in the profiler, transient memory drops measurably with aliasing, and Release starts with no
shader compilation.

## Phase 11 (optional): advanced features

- **Many lights:** ReSTIR DI for thousands of shadowed lights.
- **Better GI:** ReSTIR GI or a world-space radiance cache on top of DDGI.
- **Shadows and geometry:** virtual shadow maps; mesh shaders and a visibility buffer.
- **Output:** HDR10 / scRGB swap chain; DLSS and XeSS next to FSR.
- **Lenses:** depth of field and motion blur.
- **Test scenes:** Bistro and San Miguel, to test the renderer beyond Sponza.

## Out of scope for now

Skeletal animation, GPU particles, terrain and decals have moved out of the roadmap. The goal is the best possible
rendering of static scenes like Sponza. They can come back once the lighting pipeline is complete.
