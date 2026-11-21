# AtmosDX12 architecture

## Modules and dependencies

```text
app/ (AtmosDX12.exe)            Application (frame loop), FlyCameraController, DebugPanels
   | links
engine/ (AtmosEngine.lib)
   ui/                          DebugUi: ImGui Win32 + D3D12 backends, an IOverlayRenderer
   runtime/                     AssetManager (handles, async loading, GPU residency), RenderSceneBuilder
   graphics/                    Renderer, SceneRenderer, RenderScene, RenderSettings, RenderView, GpuScene, Mesh,
                                Material, Frustum
   graphics/lighting/           LightSetup (lights, shadow views, cascades), SkyLight (IBL), LightingTypes
   graphics/rendergraph/        RenderGraph, TransientResourcePool
   graphics/passes/             DepthPrepass, ShadowPass, GBufferPass, LightClusteringPass, DeferredLightingPass,
                                SkyPass, TransparentPass, TonemapPass (+ DepthPipelines shared by depth passes)
   graphics/d3d12/              Thin D3D12 layer: device, queues, heaps, textures, buffers, pipelines, commands
   assets/                      glTF import, model/texture cookers, asset cache -> CPU ModelData/TextureData
   scene/                       Scene, Entity, Transform, SceneSerializer (JSON), Camera
   platform/                    Window, Input, Paths (Win32)
   core/                        Log, Assert, FrameTimer, ThreadPool, Handle, string helpers
   shaders/                     HLSL, compiled at runtime by DXC:
                                  Common.hlsli    view constants, samplers, fullscreen helpers
                                  Scene/          draw/material loading, vertex processing, GBuffer encoding
                                  Lighting/       BRDF, clusters, shadows, IBL, SurfaceLighting (shared lighting)
                                  Sky/            analytic sky model
                                  Passes/         one entry-point file per pass
```

**Dependencies only point downward:**

- `runtime/` is the only module that knows both `scene/` and `graphics/`.
- `assets/` is CPU-only and thread-safe. It produces `ModelData`/`TextureData` and uses the CPU-only data headers
  `graphics/MeshData.h` and `graphics/MaterialData.h`.
- `scene/` holds asset *handles*, never GPU types.
- The renderer knows the debug UI only as an `IOverlayRenderer`.

**The app talks to the engine through:**

- `Renderer`: `Render`, `Resize`, `GetDevice`, `GetFrameStats`, `RequestFrameDump`
- `AssetManager`: `LoadModel`, `Update`, `GetStats`
- `Scene` and its serializer
- `RequestSceneAssets` and `BuildRenderScene`
- `DebugUi`, `Camera`, `Window`, `Input`

## Ownership

```text
Application
|-- Input, Window, Camera, FlyCameraController, FrameTimer
|-- Scene (entities, environment, start camera) -> RenderScene (rebuilt every frame)
|-- Renderer
|   |-- GraphicsDevice      factory, adapter, device, graphics + copy queues, 3 DescriptorHeaps,
|   |                       DeferredReleaseQueue, bindless root signature, info queue
|   |-- SwapChain           back buffers as Textures
|   |-- FrameScheduler      per-slot allocators + frame fence; drives the release queue
|   |-- UploadAllocator     per-frame linear upload memory (constants)
|   |-- CommandContext      barrier batching, target binding, root binding, draws
|   |-- ShaderCompiler      DXC
|   |-- TransientResourcePool  transient textures and buffers reused across frames
|   |-- GpuTimer            timestamp queries around the frame
|   `-- SceneRenderer       decides what is drawn
|       |-- GpuScene        per-frame DrawData + material buffers; per-view culled DrawLists
|       |-- LightSetup      GPU light list, camera + shadow views (cascades, atlas tiles)
|       |-- SkyLight        persistent sky cube, SH irradiance, prefiltered specular, BRDF LUT
|       |-- view and light FrameStructuredBuffers (per frame slot)
|       `-- passes          each owns its PipelineStates
|-- AssetManager            ThreadPool, UploadQueue (copy queue), models, textures, fallbacks
`-- DebugUi                 destroyed first; the app waits for the GPU before shutdown
```

## Application frame

```text
messages -> resize -> DebugUi::BeginFrame -> shortcuts (F1/F5) -> fly camera (RMB held, cursor locked)
-> AssetManager::Update            integrate finished jobs, record uploads, mark ready, patch material textures
-> DebugPanels::Draw -> DebugUi::EndFrame   edits environment, RenderSettings, lights; may spawn lights
-> BuildRenderScene(scene, assets) entities x model nodes -> MeshInstances, light entities -> RenderLights
-> renderScene.settings = the app's RenderSettings
-> Renderer::Render(renderScene, view, debugUi)
```

## Input

- **`Window`** turns Win32 messages into `Input` state:
  - keys
  - mouse buttons
  - wheel steps
  - raw mouse motion (`WM_INPUT`), which is unaccelerated and unaffected by screen edges or cursor clipping
- **`Input`** holds held state, plus once-per-press events (`ConsumeKeyPress`, `ConsumeMouseButtonPress`) and
  accumulated deltas, which the app consumes once per frame. Focus loss resets it.
- **`FlyCameraController`** (app) starts a flight on a right-button press that is not over the UI, and flies until
  the button is released. While flying, `Window::SetCursorLocked(true)` hides the cursor and pins it in place.
- **ImGui** sees every message first through `Window::SetMessageHandler`. `DebugUi::WantsMouse` keeps clicks on UI
  windows from starting a flight.

## Asset pipeline

```text
worker thread                                   main thread (AssetManager::Update)          GPU
LoadCookedModel(path)                           create Mesh buffers and Materials            copy queue
  cached .amodel newer than source? read it      (textures -> fallback indices)              (UploadQueue)
  else ImportGltfModel, then write .amodel      request every material texture               fence -> Ready
LoadCookedTexture(path, kind)                   create Texture (all mips), record upload
  cached .dds newer than source? read it
  else WIC decode -> mips -> BC1/BC3/BC5 -> write .dds
```

- **Handles.** `ModelHandle` is a typed index. `LoadModel` deduplicates by path, and textures are deduplicated by
  path + kind.
- **Fallbacks.** A white texture covers base color, occlusion, metal-roughness and emissive; a flat normal covers
  normal maps; a magenta cube replaces models that fail to load. Materials point at fallbacks until their textures
  are ready.
- **Cache.** `<exe>/AssetCache/` holds files named `<name>_<hash(path, cooker version)>.amodel` or `.dds`. Bump
  `FormatVersion` (models) or `CookerVersion` (textures) after changing a cooker.
- **Texture kinds.** Base color and emissive are sRGB BC1/BC3; metallic-roughness and occlusion are linear BC1;
  normal maps are BC5.

## Frame lifecycle (`Renderer::Render`)

1. `FrameScheduler::BeginFrame` waits for this frame slot's fence, frees release-queue entries the GPU has finished
   with, and opens the command list.
2. `UploadAllocator::BeginFrame` resets the slot's upload region. `CommandContext::Begin` binds the descriptor heap,
   then the root signature.
3. `ViewConstants` are bound once, for both graphics and compute. `GpuTimer::Begin` starts the frame timestamp.
4. `SceneRenderer::Prepare`:
   - `GpuScene::Update` writes a `DrawData` for every (instance, submesh) and a `MaterialGpuData` for every
     material into this slot's buffers. Nothing is culled here.
   - `LightSetup::Prepare` builds the GPU light list and the views: camera, 4 sun cascades, then one atlas tile per
     shadowed spot light and six per shadowed point light (nearest lights first).
   - `GpuScene::Cull` fills one `DrawList` per view (camera; each shadow view without transparents). Transparent
     camera draws are sorted back to front.
5. A `RenderGraph` is built: import the back buffer, `SceneRenderer::AddPasses`, then an `Overlay` pass when a
   debug UI is given. Each pass creates the transient outputs it owns and returns their handles.
6. `RenderGraph::Execute`:
   - culls unused passes
   - derives usage flags and takes transients from the `TransientResourcePool`
   - for each live pass: transitions, UAV barriers, binds targets, applies clears, runs the pass
   - leaves the back buffer in PRESENT
7. `GpuTimer::End`, Submit, Present, `EndFrame` (signal the fence and rotate the slot), then the pool's `EndFrame`
   (evicts resources unused for 3 frames). Pass list, scene stats and GPU time are kept for `GetFrameStats()`.

```text
Sky Light *         compute     Sky Light Capture, Irradiance SH, Specular mips, BRDF LUT   only when the sun moves
Shadow Cascade 0-3  depth-write Sun Shadow Cascades (2048x2048 x 4 array, one slice per pass)
Shadow Atlas        depth-write Shadow Atlas (4096x4096, 512x512 tiles)
Depth Prepass       depth-write Scene Depth                                     4 PSOs: opaque/masked x culling
GBuffer             depth-read (EQUAL) -> GBuffer A/B/C, Scene Color (cleared, emissive written)
Light Clustering    compute     Cluster Light Counts / Indices (16x9x24 clusters)
Deferred Lighting   compute     reads depth, GBuffer, shadows, clusters, IBL; adds to Scene Color (UAV)
Sky                 depth-read  Scene Color (load)
Transparent         depth-read  Scene Color (alpha blend, forward, same lighting); culled when empty
Tonemap             read        Scene Color -> Back Buffer (passthrough for debug views)
Overlay             (optional)  Back Buffer   ImGui
```

## Render graph (`graphics/rendergraph/`)

A pass is two callbacks:

```cpp
graph.AddPass(L"Sky",
    [&](RGBuilder& builder) {                 // setup: declare how resources are used
        builder.ReadDepth(depth);
        builder.WriteColor(sceneColor, LoadOp::Load);
    },
    [this, constants](RGContext& context) {   // execute: record commands
        context.commands.SetPipeline(pipeline_);
        context.commands.SetRootConstants(constants);
        context.commands.DrawFullscreenTriangle();
    });
```

- **Handles.** `RGTexture` is an index into this frame's graph. `RGContext::GetTexture(handle)` returns the real
  `Texture`, for example to read its bindless SRV index.
- **Accesses:**

  | Builder call | Resource state |
  |---|---|
  | `ReadTexture` | SRV in any stage |
  | `WriteColor(LoadOp)` | RENDER_TARGET |
  | `WriteDepth(LoadOp)` | DEPTH_WRITE |
  | `ReadDepth` | DEPTH_READ, with a read-only DSV |
  | `ReadWriteTexture(t, mip)` | UAV, one mip or all |
  | `ReadBuffer` / `ReadWriteBuffer` | SRV / UAV |

  Several reads of one resource in one pass combine into one state. `WriteDepth` takes an optional array slice, so
  each cascade is its own pass with its own clear. Consecutive UAV accesses get a UAV barrier.
- **Buffers.** `CreateBuffer`/`ImportBuffer` work like textures. Buffers decay to COMMON after each submit, so the
  graph treats them as COMMON on their first use in a frame.
- **Culling.** The graph walks the passes backwards.
  - A resource is *needed* if it is imported, or a later live pass reads it.
  - `LoadOp::Load` counts as a read.
  - A pass lives if it has side effects or writes something needed.
  - A full overwrite (`Clear`/`DontCare`) ends the need for earlier writers.
- **Transients** get their usage flags from how live passes access them. They are taken from the
  `TransientResourcePool`, matched by size, format, usage and clear values (buffers: size, stride, usage). Resize needs no code: next frame asks for the new size, and old
  textures age out.
- **Execution order** is declaration order. State tracking stays in `Texture` and `CommandContext`; the graph only
  decides *when* to transition.

## Geometry and materials

- **`Mesh`** has three static `Buffer`s:
  - positions: `float3`, raw view
  - attributes: normal + tangent + uv, 36 B, raw view
  - indices: `uint32`

  It also keeps a submesh table (index range + material index into the model's materials) and local bounds.
- **`Material`** holds `MaterialData`: glTF metallic-roughness factors, alpha mode and cutoff, double-sided, texture
  paths. It also holds the bindless SRV index of each of its 5 texture slots. Shaders see `MaterialGpuData`, 64 B.
- **Vertex pulling.** `Scene/SceneData.hlsli` reads vertices by `SV_VertexID` from `ByteAddressBuffer`s. Only the index buffer
  goes through the input assembler.
- **Per draw:** root constants `{ drawIndex, drawBufferIndex, materialBufferIndex, positionBufferIndex,
  attributeBufferIndex, viewBufferIndex, viewIndex }`. `DrawData { world, normalMatrix, materialIndex }`, the material
  table and the `ViewData` array are `StructuredBuffer`s in the frame slot's memory. The same mesh shaders render the
  camera and every shadow view; only `viewIndex` changes.
- **Material evaluation** (`Scene/MaterialSurface.hlsli`) is shared by the GBuffer and transparent passes: base
  color, BC5 normal map with Z rebuilt, metallic-roughness, occlusion, emissive. Back faces of double-sided
  materials flip their tangent frame.
- **Depth.** The prepass writes depth (masked buckets `clip` against base color alpha). The GBuffer tests `EQUAL`,
  read-only, with no alpha test. Both use one `precise` transform. Sky sits at z = 0 with `GREATER_EQUAL`, so it only
  covers cleared depth.

## Lighting (`graphics/lighting/`, `shaders/Lighting/`)

- **GBuffer** (`Scene/GBuffer.hlsli`):

  | Target | Format | Contents |
  |---|---|---|
  | A | RGBA8 sRGB | base color, ambient occlusion |
  | B | R10G10B10A2 | octahedral normal, shading model |
  | C | RGBA8 | metallic, roughness |
  | Scene Color | RGBA16F | emissive, then lighting is added |

- **BRDF:** GGX distribution, height-correlated Smith visibility, Schlick Fresnel, Lambert diffuse
  (`Lighting/BRDF.hlsli`). `Lighting/SurfaceLighting.hlsli` sums sun + clustered local lights + IBL. It is the only
  lighting code: the deferred and transparent passes both call `EvaluateLighting`.
- **Units.** Sun intensity is illuminance; light intensity is luminous intensity with inverse-square falloff windowed
  to zero at `range`. The visible sun disk uses intensity / pi, so a lit white surface matches the sky's scale.
- **Clustered lights.** A 16x9 screen grid x 24 logarithmic depth slices (0.1-500 m). `LightClustering.hlsl` runs
  one group per cluster, tests every light's sphere against the cluster's view-space AABB, and stores up to 128
  indices per cluster.
- **Sun shadows.** 4 stable cascades (bounding sphere + texel snapping, practical split lambda), reverse-Z ortho,
  depth clip off so casters behind the near plane pancake. Normal offset in texels + negative raster depth bias,
  3x3 PCF with a comparison sampler (s4).
- **Local shadows.** One 4096x4096 atlas with 512x512 tiles; spot lights take one tile, point lights six (cube faces
  as 90-degree frusta). The shadowed lights nearest the camera get tiles first.
- **IBL** (`SkyLight`). When the sun direction changes, the sky is captured into a 64x64 cube, projected to SH9
  (pre-convolved for diffuse) and GGX-prefiltered into a 128x128 cube with 6 mips. A split-sum BRDF LUT is built once.
- **`LightingConstants`** (b2, mirrored in `Lighting/LightingCommon.hlsli`) carry every index and parameter the
  lighting code needs, so the deferred and transparent passes bind the same block.
- **Debug views** (`RenderSettings::debugView`): base color, normals, roughness, metallic, AO, shadow cascades and
  the light-count heat map. The deferred pass writes them, and tonemapping passes them through.

## Binding model (bindless)

There is one root signature for every pipeline (`BindlessRootSignature.h`):

| Slot | Register | Contents |
|---|---|---|
| `RootConstants` | b0 | Up to 16 dwords per pass or draw, usually bindless indices |
| `ViewConstants` | b1 | `ViewConstants`, bound once per frame |
| `PassConstants` | b2 | Per-pass CBV for larger data (e.g. `LightingConstants`) |
| static samplers | s0–s4 | PointClamp, LinearClamp, LinearWrap, AnisotropicWrap, ShadowCompare (GREATER_EQUAL) |

## Resource lifetime

- **Destroyed textures and buffers** go to `GraphicsDevice::GetReleaseQueue()`. Their resource and descriptors are
  tagged with the frame-fence value of the current frame and freed once the GPU passes it. Destroying a resource
  mid-frame is safe.
- **Swap-chain resize** flushes and calls `ReleaseAll()`, because DXGI needs every back-buffer reference gone.
- **Static buffers** stay in COMMON. They rely on implicit promotion (copy dest, SRV, index buffer) and decay, so
  they need no barriers. Textures are tracked explicitly.
- **Per-frame data** (constants, `DrawData`, materials, views, lights) lives in memory owned by the current frame slot and is
  protected by the same fence wait as the command allocator.
- **Asset uploads** run on the copy queue (`UploadQueue`). The CPU waits for its fence before any frame references the
  resource, so the copy and graphics queues never touch one resource at the same time. Destinations rely on COMMON
  promotion and decay.

## Conventions

- **Coordinates:** left-handed, +Y up, camera yaw 0 looks along +Z. Front faces wind **clockwise**.
- **glTF import** mirrors Z on positions, normals, tangents and node matrices, negates the tangent W, and reverses
  winding. A mirror keeps the
  projected image the same, so glTF's counter-clockwise faces would otherwise stay counter-clockwise on screen.
- **Depth:** reverse-Z with an infinite far plane (near = 1, far = 0), `GREATER_EQUAL`, clear to 0.
- **Matrices** are row-major with row vectors on both sides (DXC `-Zpr`, `mul(v, M)`).
- **CPU/HLSL mirrors:** every CPU struct mirrored in HLSL is commented "Mirrors …" on both sides.
- **Errors:** `ThrowIfFailed` and exceptions for environment and API failures; `ATMOS_ASSERT` for bugs.
- **Debugging:** every D3D12 object has a debug name, and every graph pass gets a PIX/RenderDoc event.

## Design decisions

| Area | Chosen | Alternatives | Who uses what |
|---|---|---|---|
| Graphics API layer | D3D12 only, thin wrappers | Multi-API RHI | Unreal (RHI), Unity (GfxDevice), bgfx, The Forge are multi-API |
| Resource binding | Bindless SM 6.6, one root signature | Per-pass root signatures + descriptor tables | Doom Eternal and The Forge are bindless; Unreal 5 has it as an option |
| Shader compiler | DXC at runtime | FXC, Slang, offline compilation | Shipping engines compile offline (Unreal DDC, Unity variants); offline comes in Phase 6 |
| Command recording | `CommandContext` with tracked states | Hand-written barriers | MiniEngine (`CommandContext` + `GpuResource`) |
| Frame orchestration | Per-frame render graph, setup and execute lambdas | Unreal RDG parameter structs; retained graphs; Godot 4.3's command-level graph | Frostbite FrameGraph, Unity RenderGraph, Filament |
| Transient memory | Pooled committed textures | Placed + aliased resources | Frostbite, Unreal RDG, HDRP alias; v2 here (Phase 5) |
| Vertex input | Vertex pulling, separate position stream | Input-assembler layouts | Nanite and GPU-driven pipelines pull; Unreal splits positions out |
| Visibility | Depth prepass + `EQUAL` | No prepass; visibility buffer | HDRP, URP depth priming, Unreal `r.EarlyZPass`, Doom 2016 |
| Per-draw data | Per-frame `DrawData` structured buffer + root-constant index | CBV per draw; persistent GPU scene | Unreal GPUScene (Phase 5 here) |
| Culling | Brute-force CPU frustum test over a flat array | BVH/octree; GPU culling | Unreal brute-forces in parallel; Godot uses a BVH |
| Resource release | Fence-tagged deferred release queue | Flush before destroy; ref-counted RHI | Unreal defers RHI deletes until the GPU is done |
| Third-party code | CMake `FetchContent`, pinned | vcpkg, Conan, vendoring | Godot and Unreal vendor theirs |
| glTF | cgltf | fastgltf, tinygltf, Assimp | Filament gltfio, raylib |
| Depth | Reverse-Z, infinite far | Standard Z | Unreal, Unity, Frostbite |
| Matrices | Row vectors (`-Zpr`) | Column-major + transposes | Unreal, DirectXMath |
| Packaging | Static lib + exe | Engine DLLs | Unreal editor modules |
| Scene model | Plain entities + optional components | ECS (EnTT, DOTS, Bevy); actor/component graphs | Unreal actors, Unity GameObjects |
| Scene -> renderer | Rebuild `RenderScene` every frame | Persistent proxies with dirty tracking | Unreal `FScene` proxies |
| Asset pipeline | Cook on first use into a hashed cache | Explicit offline cook | Unreal cooking, Unity Library |
| Texture compression | BC1/BC3 color, BC1 data, BC5 normals | BC7 (needs a GPU compressor), Basis/KTX2 | Unreal's default compression is BC1/BC3 + BC5 |
| Async loading | Thread pool (CPU) + copy queue (GPU), resources created on the main thread | Worker-thread resource creation; DirectStorage | Copy-queue streaming in Unreal and Unity |
| Materials | Per-frame material table + index per draw | Per-draw CBV; persistent material buffer | Unity SRP Batcher, Unreal persistent material data |
| Alpha modes | Pipeline buckets; alpha test only in the depth prepass | Dynamic branching; full PSO keys | Unreal/HDRP masked depth prepass |
| Debug UI | Dear ImGui behind `IOverlayRenderer` | Custom retained UI | Ubiquitous in engine tooling |
| Lighting path | Deferred GBuffer + compute lighting | Forward+; visibility buffer | Unreal, HDRP, Frostbite are deferred; Doom 2016 is forward+ |
| Light culling | Clustered (froxels) in compute | Tiled 2D; per-object lists | HDRP, Doom 2016, Unreal (forward) |
| Sun shadows | 4 stable CSM, PCF | Virtual shadow maps; ray tracing | Unreal 5 VSM; Unity and HDRP CSM |
| Local shadows | One depth atlas, a tile per face | Cube-map arrays | HDRP and Unreal atlases |
| Ambient | Sky IBL: SH9 + prefiltered cube + BRDF LUT | Constant ambient; probes | Unity SH ambient, Unreal sky light |
| Transparency | Forward pass, back to front, shared lighting code | Order-independent transparency | Most engines, including Unreal and HDRP |
| Scene files | JSON, paths relative to the file | YAML (Unity), binary packages (Unreal), USD | |

## Known limits (intentional for now)

- Shadow maps are re-rendered every frame (no static caching); no contact shadows or soft penumbrae.
- IBL comes from the analytic sky only: no reflection or irradiance probes, no specular occlusion.
- Transparents are sorted per draw, not per triangle; one blend mode (alpha), no refraction.
- At most 1,024 lights per frame and 128 per cluster; extra lights in a full cluster are dropped.
- Textures embedded in `.glb` buffers or data URIs are not loaded yet; they fall back to white.
- Texture compression is BC1/BC3/BC5 on the CPU; BC7 needs a GPU compressor.
- The asset cache lives next to each executable, so Debug and Release cook separately. Stale cache files are not
  cleaned up.
- No asset unloading or hot reload yet. Single-threaded command recording on one direct queue.
- `GpuScene` holds at most 16,384 draws and 4,096 materials per frame, and logs a warning beyond that.
- The render graph does not alias memory or reorder passes, and runs everything on the direct queue.
