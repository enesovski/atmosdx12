# AtmosDX12 architecture

## Modules and dependencies

```text
app/ (AtmosDX12.exe)            Application, FlyCameraController: window loop, input, scene setup
   │ links
engine/ (AtmosEngine.lib)
   graphics/                    Renderer, RenderScene, RenderView, GpuScene, Mesh/MeshData, Frustum
   graphics/rendergraph/        RenderGraph, TexturePool
   graphics/passes/             DepthPrepass, OpaquePass, SkyPass, TonemapPass
   graphics/d3d12/              Thin D3D12 layer: device, heaps, textures, buffers, pipelines, commands
   assets/                      glTF loader (cgltf), primitive meshes → MeshData (CPU only)
   scene/                       Camera (no GPU types)
   platform/                    Window, Input, Paths (Win32)
   core/                        Log, Assert, FrameTimer, Build flags, string helpers
   shaders/                     HLSL, compiled at runtime by DXC
```

**Dependencies only point downward:**

- `d3d12/` knows nothing about the render graph, passes, meshes, or the renderer.
- `rendergraph/` knows `d3d12/` but no specific pass.
- `assets/` produces CPU `MeshData` and never touches the GPU.
- `scene/`, `platform/` and `core/` know nothing about graphics.

**The app talks to the engine only through:**

- `Renderer`: `Render`, `Resize`, `CreateMeshes`, `RequestFrameDump`
- `RenderScene`, `RenderView`
- `Camera`, `Window`, `Input`
- the asset loaders

## Ownership

```text
Application
├── Input, Window, Camera, FlyCameraController, FrameTimer, RenderScene
├── Renderer
│   ├── GraphicsDevice      factory, adapter, device, graphics queue, 3 DescriptorHeaps,
│   │                       DeferredReleaseQueue, bindless root signature, info queue
│   ├── SwapChain           back buffers as Textures
│   ├── FrameScheduler      per-slot allocators + frame fence; drives the release queue
│   ├── UploadAllocator     per-frame linear upload memory (constants)
│   ├── CommandContext      barrier batching, target binding, root binding, draws
│   ├── UploadContext       load-time copies into GPU-only buffers
│   ├── ShaderCompiler      DXC
│   ├── TexturePool         transient render targets reused across frames
│   ├── GpuScene            per-frame draw list + DrawData buffers (one per frame slot)
│   └── DepthPrepass, OpaquePass, SkyPass, TonemapPass (each owns its PipelineState)
└── meshes_                 GPU Meshes (destroyed before the Renderer)
```

## Frame lifecycle (`Renderer::Render`)

1. `FrameScheduler::BeginFrame` waits for this frame slot's fence, frees release-queue entries the GPU has finished
   with, and opens the command list.
2. `UploadAllocator::BeginFrame` resets the slot's upload region. `CommandContext::Begin` binds the descriptor heap,
   then the root signature.
3. `ViewConstants` are bound once, for both graphics and compute.
4. `GpuScene::Update` frustum-culls `RenderScene::meshInstances` and writes a `DrawData` per visible
   (instance, submesh) into this slot's upload buffer.
5. A `RenderGraph` is built:
   - import the back buffer
   - create the transient `Scene Depth` and `Scene Color`
   - each pass calls `AddToGraph`
6. `RenderGraph::Execute`:
   - culls unused passes
   - derives texture usage flags
   - takes transients from the `TexturePool`
   - for each live pass: transitions, binds targets, applies clears, runs the pass
   - leaves the back buffer in PRESENT
7. Submit, Present, `EndFrame` (signal the fence and rotate the slot), then `TexturePool::EndFrame` (evicts textures
   unused for 3 frames).

```text
Depth Prepass   depth-write(clear)  Scene Depth
Opaque          depth-read          Scene Depth   color(clear)  Scene Color
Sky             depth-read          Scene Depth   color(load)   Scene Color
Tonemap         read                Scene Color   color(dont-care) Back Buffer
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

## Geometry path

- **`MeshData`** (CPU) is produced by the asset loaders in engine space.
- **`Mesh`** (GPU) has three static `Buffer`s:
  - positions: `float3`, raw view
  - attributes: normal + uv, 20 B, raw view
  - indices: `uint32`

  It also keeps a submesh table (index range + base color) and local bounds.
- **Vertex pulling.** `Mesh.hlsl` reads vertices by `SV_VertexID` from `ResourceDescriptorHeap[...]`
  `ByteAddressBuffer`s. Only the index buffer goes through the input assembler.
- **Per draw:** root constants `{ drawIndex, drawBufferIndex, positionBufferIndex, attributeBufferIndex }`
  (`MeshRootConstants`), and `DrawData { world, normalMatrix, baseColor }` in a `StructuredBuffer`.
- **Depth.** The prepass writes depth. Opaque tests `EQUAL` against it, read-only. Both use one `precise` transform,
  so their depths match bit for bit. Sky sits at z = 0 with `GREATER_EQUAL`, so it only covers cleared depth.

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
- **Per-frame data** (constants, `DrawData`) lives in memory owned by the current frame slot and is protected by the
  same fence wait as the command allocator.

## Conventions

- **Coordinates:** left-handed, +Y up, camera yaw 0 looks along +Z. Front faces wind **clockwise**.
- **glTF import** mirrors Z on positions, normals and node matrices, and reverses winding. A mirror keeps the
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

## Known limits (intentional for now)

- Opaque lighting is a temporary lambert + sun. Deferred PBR + clustered lights come in Phase 3.
- Materials are a base color only: no textures, tangents or alpha. These arrive in Phase 2.
- glTF is parsed at load time, and uploads block, on the graphics queue. A cooked format and async copies come in
  Phase 2.
- Single-threaded recording on one direct queue. Barriers cover whole resources.
- `GpuScene` holds at most 16,384 draws per frame, and logs a warning beyond that.
- The render graph does not alias memory or reorder passes, and it only handles textures. Graph-managed buffers
  arrive when a pass needs one.
