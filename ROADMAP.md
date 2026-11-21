# AtmosDX12 roadmap

This roadmap covers going from today's sky renderer to a general-purpose real-time render engine. The target is the
rendering layer of something like Unity HDRP or Unreal: geometry, materials, lighting, shadows, a physically based
atmosphere, and post-processing, all driven by a scene.

Each phase ends with a "done when" check. Finish phases in order. Later phases assume that the earlier exit criteria
hold.

| Phase | Topic | Status |
|---|---|---|
| 0 | Foundation refactor | ✅ done |
| 1 | Render graph v1 and first geometry | ✅ done ([plan and decisions](docs/phase-1-plan.md)) |
| 2 | Scene, assets, and materials | ✅ done ([decisions](docs/phase-2-plan.md)) |
| 3 | Lighting and shadows (deferred + clustered) | ✅ done ([plan and decisions](docs/phase-3-plan.md)) |
| 4 | Physically based atmosphere and post-processing | |
| 5 | GPU-driven rendering and performance | |
| 6 | Content features and engine polish | |
| 7 | Optional: ray tracing, GI, virtual textures | |

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

Render graph **v2** (Phase 5) adds:

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
- **Stats:** per-pass stats show live/culled passes, draws and materials. GPU timings come in Phase 5.
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

## Phase 4: atmosphere and post-processing

The project's namesake: a physically based sky that drives all outdoor lighting.

**Steps:**

1. **Atmosphere** (Hillaire 2020):
   - transmittance, multiple-scattering and sky-view LUTs
   - an aerial-perspective froxel volume applied to opaque and transparent geometry
   - sun and ambient color taken from transmittance
   - IBL capture switches to the physical sky
2. **Motion vectors:** camera reprojection from depth, plus a velocity pass for moving objects (previous transforms
   in `GpuScene`).
3. **TAA:**
   - Halton jitter in the projection
   - history buffer imported into the graph
   - YCoCg neighborhood clipping
   - velocity-weighted feedback
4. **Volumetric fog:** froxels (Wronski 2014 / Hillaire 2015), lit by the sun shadow map and the clustered lights,
   temporally reprojected.
5. **Volumetric clouds:**
   - raymarched (Nubis-style) at quarter resolution with temporal reconstruction
   - weather map
   - cloud shadows projected onto the scene
6. **Post chain:**
   - histogram auto exposure
   - physically based bloom (downsample/upsample chain)
   - depth of field
   - motion blur
   - color grading LUT
   - ACES / AgX tonemapping (replacing `1 - exp(-x)`)
7. **Screen-space effects:** GTAO (Jimenez 2016) and hierarchical SSR with an IBL fallback.

**Done when:** a sun sweep from -10° to 90° looks plausible from sunrise to twilight, with aerial perspective, fog
shafts, clouds, and stable TAA during camera motion.

## Phase 5: GPU-driven rendering and performance

**Steps:**

1. **Profiling first:** per-pass GPU timestamps in the render graph, shown in the overlay; Tracy for CPU zones.
2. **Render graph v2:**
   - transient memory aliasing with placed resources
   - an async compute queue (lighting/fog overlap with shadows)
   - split or enhanced barriers
3. **Persistent GPU scene:**
   - instance and material data resident on the GPU, updated by deltas (Unreal GPUScene)
   - a unified geometry buffer for all meshes
4. **GPU culling:**
   - per-instance frustum culling
   - two-phase HZB occlusion culling
   - `ExecuteIndirect` draws for every view (camera, cascades, atlas tiles)
5. **Geometry scaling:** mesh LODs (meshoptimizer), meshlets with mesh shaders, and a visibility-buffer evaluation.
6. **Parallel CPU:**
   - a job system in `core/`
   - parallel command-list recording per graph pass
   - parallel `GpuScene` updates
7. **Shader hot reload:** file watcher → recompile → swap only the affected PSOs.

**Done when:** 100k+ instances are GPU-culled at interactive rates, per-pass GPU timings are visible, async compute
overlaps lighting, and transient memory drops measurably with aliasing.

## Phase 6: content features and engine polish

**Steps:**

1. **Animation:** glTF skins and animations, GPU skinning in compute, and motion vectors for skinned meshes.
2. **GPU particles:** compute emit/simulate/compact, sorted transparent rendering, and lighting from clusters.
3. **Deferred decals:** box projection into the GBuffer.
4. **Terrain:** heightmap clipmaps with a material blend, suited to the outdoor atmosphere focus.
5. **Display output:** HDR10 / scRGB swap chain with a matching tonemap path.
6. **Asset pipeline maturity:**
   - BC7 through a GPU compressor
   - `.glb` and embedded images
   - MikkTSpace tangents
   - an on-disk PSO cache and offline shader compilation for Release
7. **Editor tooling (ImGui):**
   - scene hierarchy
   - inspector for components
   - transform gizmos (ImGuizmo)
   - asset browser
   - render-graph viewer
   - undo/redo of scene edits

**Done when:** an animated character walks across a terrain under the dynamic sky, scenes can be edited and saved
entirely in the app, and a Release build starts with no shader compilation.

## Phase 7 (optional): advanced features

- **Ray tracing (DXR 1.1):** inline ray-query shadows, reflections and AO with a denoiser, built on the GPU scene's
  acceleration structures.
- **Global illumination:** dynamic diffuse GI with probes (DDGI-style) or screen-space GI as a fallback.
- **Virtual textures and virtual shadow maps:** once GPU-driven rendering is in place.
- **Upscaling:** FSR / DLSS / XeSS on top of TAA.
