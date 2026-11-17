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
| 2 | Scene, assets, and materials | |
| 3 | Lighting and shadows (deferred + clustered) | |
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

## Phase 2: scene, assets, and materials

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

## Phase 3: lighting and shadows

- **Deferred GBuffer:** albedo, octahedral normal, roughness, metalness, AO, emissive, and motion vectors.
- **BRDF:** Cook-Torrance GGX with energy-conserving diffuse.
- **Lights:** directional (the sun, shared with the atmosphere), point, and spot.
- **Clustered light culling** in compute (Olsson 2012), used by deferred lighting and forward transparents.
- **Shadows:** cascaded shadow maps for the sun (stable cascades, PCF / PCSS-lite) and a shadow atlas for local
  lights.
- **IBL:** sky captured into a cubemap, prefiltered specular, irradiance SH, and a BRDF LUT.
- **Transparents:** a forward pass, sorted back to front.

**Done when:** Sponza is lit by the sun with cascaded shadows and 100+ point lights at interactive frame rates, and
IBL matches the sky.

## Phase 4: atmosphere and post-processing

- **Atmosphere (Hillaire 2020):**
  - transmittance, multiple-scattering, and sky-view LUTs
  - an aerial-perspective froxel volume
  - sun color from transmittance
- **Volumetric fog:** froxel-based (Wronski 2014 / Hillaire 2015), temporally reprojected.
- **Volumetric clouds:** raymarched (Nubis-style), temporal reprojection at reduced resolution, and cloud shadows.
- **Post-processing:**
  - histogram auto exposure
  - bloom
  - TAA
  - depth of field
  - motion blur
  - color grading LUT
  - ACES / AgX tonemapping
- **Screen-space effects:** GTAO (Jimenez 2016) and SSR with an IBL fallback.

**Done when:** a sun sweep from -10° to 90° looks plausible from sunrise to twilight, with fog, clouds, and stable
TAA.

## Phase 5: GPU-driven rendering and performance

- Render graph v2: aliasing, async compute, and split or enhanced barriers.
- A persistent GPU scene buffer (Unreal GPUScene-style) and GPU culling (frustum + two-phase HZB occlusion) with
  `ExecuteIndirect`.
- A unified geometry buffer, instancing, LODs, and optionally meshlets with mesh shaders. Revisit a visibility
  buffer here.
- A per-pass GPU timestamp profiler, plus Tracy/PIX CPU profiling.
- Multithreaded command recording through a job system.
- Shader hot reload.

**Done when:** 100k+ instances run GPU-culled, per-pass timings are visible, and async compute overlaps lighting.

## Phase 6: content features and engine polish

- Skinned meshes and GPU skinning.
- GPU particles.
- Deferred decals.
- Terrain.
- HDR display output.
- An on-disk PSO cache and offline shader builds.
- ImGui editor tooling: hierarchy, inspector, gizmos, and a render-graph viewer.

## Phase 7 (optional): advanced features

- DXR shadows, reflections, and AO.
- DDGI-style dynamic diffuse GI.
- Virtual textures and shadow maps.
- FSR / DLSS upscaling.
