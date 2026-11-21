# AtmosDX12 architecture

## Modules and dependencies

```text
app/ (AtmosDX12.exe)            Application (frame loop), FlyCameraController, DebugPanels
   | links
engine/ (AtmosEngine.lib)
   ui/                          DebugUi: ImGui Win32 + D3D12 backends, an IOverlayRenderer
   runtime/                     AssetManager (handles, async loading, GPU residency), RenderSceneBuilder
   graphics/                    Renderer, RenderScene, RenderView, GpuScene, Mesh, Material, Frustum
   graphics/rendergraph/        RenderGraph, TexturePool
   graphics/passes/             DepthPrepass, OpaquePass, SkyPass, TonemapPass
   graphics/d3d12/              Thin D3D12 layer: device, queues, heaps, textures, buffers, pipelines, commands
   assets/                      glTF import, model/texture cookers, asset cache -> CPU ModelData/TextureData
   scene/                       Scene, Entity, Transform, SceneSerializer (JSON), Camera
   platform/                    Window, Input, Paths (Win32)
   core/                        Log, Assert, FrameTimer, ThreadPool, Handle, string helpers
   shaders/                     HLSL, compiled at runtime by DXC
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
|   |-- TexturePool         transient render targets reused across frames
|   |-- GpuScene            per-frame draw buckets + DrawData and material buffers (per frame slot)
|   `-- DepthPrepass, OpaquePass, SkyPass, TonemapPass (each owns its PipelineStates)
|-- AssetManager            ThreadPool, UploadQueue (copy queue), models, textures, fallbacks
`-- DebugUi                 destroyed first; the app waits for the GPU before shutdown
```

## Application frame

```text
messages -> resize -> DebugUi::BeginFrame -> shortcuts (F1/F5) -> fly camera (RMB held, cursor locked)
-> AssetManager::Update            integrate finished jobs, record uploads, mark ready, patch material textures
-> BuildRenderScene(scene, assets) entities x model nodes -> MeshInstances (+ environment)
-> DebugPanels::Draw -> DebugUi::EndFrame
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
3. `ViewConstants` are bound once, for both graphics and compute.
4. `GpuScene::Update` frustum-culls `RenderScene::meshInstances`. For each visible (instance, submesh) it writes a
   `DrawData`, and for each material in use a `MaterialGpuData`, into this slot's buffers. It then sorts draws into
   buckets: opaque or masked, single- or double-sided.
5. A `RenderGraph` is built:
   - import the back buffer
   - create the transient `Scene Depth` and `Scene Color`
   - each pass calls `AddToGraph`
   - an `Overlay` pass is added when a debug UI is given
6. `RenderGraph::Execute`:
   - culls unused passes
   - derives texture usage flags
   - takes transients from the `TexturePool`
   - for each live pass: transitions, binds targets, applies clears, runs the pass
   - leaves the back buffer in PRESENT
7. Submit, Present, `EndFrame` (signal the fence and rotate the slot), then `TexturePool::EndFrame` (evicts textures
   unused for 3 frames). The pass list and scene statistics are kept for `GetFrameStats()`.

```text
Depth Prepass   depth-write(clear)  Scene Depth                                   4 PSOs: opaque/masked x culling
Opaque          depth-read (EQUAL)  Scene Depth   color(clear)     Scene Color    2 PSOs: single/double-sided
Sky             depth-read          Scene Depth   color(load)      Scene Color
Tonemap         read                Scene Color   color(dont-care) Back Buffer
Overlay         (optional)                        color(load)      Back Buffer    ImGui
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
  | `ReadWriteTexture` | UAV |

  Several reads of one texture in one pass combine into one state.
- **Culling.** The graph walks the passes backwards.
  - A resource is *needed* if it is imported, or a later live pass reads it.
  - `LoadOp::Load` counts as a read.
  - A pass lives if it has side effects or writes something needed.
  - A full overwrite (`Clear`/`DontCare`) ends the need for earlier writers.
- **Transients** get their usage flags from how live passes access them. They are taken from the `TexturePool`,
  matched by size, format, usage and clear values. Resize needs no code: next frame asks for the new size, and old
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
- **Vertex pulling.** `Mesh.hlsl` reads vertices by `SV_VertexID` from `ByteAddressBuffer`s. Only the index buffer
  goes through the input assembler.
- **Per draw:** root constants `{ drawIndex, drawBufferIndex, materialBufferIndex, positionBufferIndex,
  attributeBufferIndex }`. `DrawData { world, normalMatrix, materialIndex }` and the material table are
  `StructuredBuffer`s in the frame slot's upload memory.
- **Shading (temporary until Phase 3):**
  - base color x factor
  - BC5 normal map with the Z channel rebuilt
  - lambert sun + ambient x occlusion + emissive
  - back faces of double-sided materials flip their tangent frame
- **Depth.** The prepass writes depth (masked buckets `clip` against base color alpha). Opaque tests `EQUAL`,
  read-only, with no alpha test. Both use one `precise` transform. Sky sits at z = 0 with `GREATER_EQUAL`, so it only
  covers cleared depth.

## Binding model (bindless)

There is one root signature for every pipeline (`BindlessRootSignature.h`):

| Slot | Register | Contents |
|---|---|---|
| `RootConstants` | b0 | Up to 16 dwords per pass or draw, usually bindless indices |
| `ViewConstants` | b1 | `ViewConstants`, bound once per frame |
| `PassConstants` | b2 | Per-pass CBV for larger data (e.g. `OpaquePassConstants`) |
| static samplers | s0–s3 | PointClamp, LinearClamp, LinearWrap, AnisotropicWrap |

## Resource lifetime

- **Destroyed textures and buffers** go to `GraphicsDevice::GetReleaseQueue()`. Their resource and descriptors are
  tagged with the frame-fence value of the current frame and freed once the GPU passes it. Destroying a resource
  mid-frame is safe.
- **Swap-chain resize** flushes and calls `ReleaseAll()`, because DXGI needs every back-buffer reference gone.
- **Static buffers** stay in COMMON. They rely on implicit promotion (copy dest, SRV, index buffer) and decay, so
  they need no barriers. Textures are tracked explicitly.
- **Per-frame data** (constants, `DrawData`, materials) lives in memory owned by the current frame slot and is
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
| Scene files | JSON, paths relative to the file | YAML (Unity), binary packages (Unreal), USD | |

## Known limits (intentional for now)

- Lighting is a temporary lambert + sun + flat ambient. There are no shadows; metallic and roughness are loaded but
  unused. Phase 3 brings deferred PBR, clustered lights and shadows.
- `BLEND` materials render as alpha-tested until Phase 3 adds a sorted transparent pass.
- Textures embedded in `.glb` buffers or data URIs are not loaded yet; they fall back to white.
- Texture compression is BC1/BC3/BC5 on the CPU; BC7 needs a GPU compressor.
- The asset cache lives next to each executable, so Debug and Release cook separately. Stale cache files are not
  cleaned up.
- No asset unloading or hot reload yet. Single-threaded command recording on one direct queue.
- `GpuScene` holds at most 16,384 draws and 4,096 materials per frame, and logs a warning beyond that.
- The render graph does not alias memory or reorder passes, and it only handles textures.
