# AtmosDX12 architecture

## Modules and dependencies

```text
app/ (AtmosDX12.exe)           Application: window loop, camera input, owns the RenderScene
   │ links
engine/ (AtmosEngine.lib)
   graphics/                    Renderer, RenderScene, RenderView, ShaderInterop
   graphics/passes/             SkyPass, TonemapPass
   graphics/d3d12/              Thin D3D12 layer: device, heaps, textures, pipelines, commands
   scene/                       Camera (gameplay-side, no GPU types)
   platform/                    Window, Input, Paths (Win32)
   core/                        Log, Assert, Build flags, string helpers
   shaders/                     HLSL, compiled at runtime by DXC
```

Dependencies only point downward: `d3d12/` knows nothing about passes, the renderer or the
scene, and `scene/`, `platform/` and `core/` know nothing about graphics. The app talks
to the engine only through `Renderer`, `RenderScene`, `RenderView`, `Camera`, `Window` and
`Input`.

## Ownership

```text
Application
├── Input, Window, Camera, RenderScene
└── Renderer
    ├── GraphicsDevice      factory, adapter, device, graphics queue,
    │                       3 DescriptorHeaps, bindless root signature, info queue
    ├── SwapChain           back buffers as Textures
    ├── FrameScheduler      per-slot allocators + fence, one command list
    ├── UploadAllocator     per-frame linear upload memory
    ├── CommandContext      barrier batching, root binding, draw/dispatch
    ├── ShaderCompiler      DXC
    ├── Texture hdrTarget_  size-dependent; recreated on resize
    └── SkyPass, TonemapPass (each owns one PipelineState)
```

Members are declared in dependency order. `Renderer::~Renderer` flushes the GPU before any
member is destroyed.

## Binding model (bindless)

There is one root signature for every pipeline (`BindlessRootSignature.h`):

| Slot | Register | Contents |
|---|---|---|
| `RootConstants` | b0 | Up to 16 dwords per pass or draw, usually bindless indices |
| `ViewConstants` | b1 | `ViewConstants` CBV, bound once per frame |
| `PassConstants` | b2 | Optional per-pass CBV for larger data |
| static samplers | s0–s3 | PointClamp, LinearClamp, LinearWrap, AnisotropicWrap |

Textures and buffers are never bound. A texture's SRV/UAV lives in the single shader-visible
heap, and shaders fetch it with `ResourceDescriptorHeap[index]`. That index comes from
`Texture::GetSrvIndex()`, passed through root constants.

## Frame lifecycle (`Renderer::Render`)

1. `FrameScheduler::BeginFrame` waits on the current slot's fence and resets its allocator.
2. `UploadAllocator::BeginFrame` resets that slot's upload region, which the same fence
   protects.
3. `CommandContext::Begin` sets the descriptor heap, then the root signatures.
4. `RecordFrame` binds `ViewConstants` and records the passes:
   `SkyPass → hdrTarget_`, then `TonemapPass(hdrTarget_) → back buffer`, then a transition to
   PRESENT.
5. `End` (flush barriers), `Submit`, `Present`, `EndFrame` (signal the fence and rotate the
   slot).
6. Debug-layer messages are forwarded to the log. If the device is lost, DRED data is logged.

## Resource state tracking

Each `Texture` stores its current state. Only `CommandContext` changes it (it is a friend):
`Transition()` queues a barrier and updates the state. Queued barriers are submitted in one
batch before the next draw, dispatch or clear. Passes call `SetRenderTarget` (which
transitions to RT) and `Transition(input, PIXEL_SHADER_RESOURCE)` for what they read. The
Phase 1 render graph will take over these calls.

## Conventions

- Left-handed, +Y up, camera yaw 0 looks along +Z. Sun azimuth turns from +Z toward +X.
- Reverse-Z with an infinite far plane: depth 1 is the near plane and 0 is infinity. Depth
  tests use `GREATER_EQUAL`, and depth clears to 0.
- Matrices are row-major on both sides (DXC `-Zpr`). HLSL uses `mul(vector, matrix)`, like
  DirectXMath.
- Every CPU struct mirrored in HLSL is marked "Mirrors …" on both sides. `ShaderInterop.h`
  pins `ViewConstants` with `static_assert`.
- Errors: environment and API failures throw (`ThrowIfFailed` → `DxException` with HRESULT and
  call site). Programmer errors use `ATMOS_ASSERT` (Debug only).
- Every D3D12 object gets a debug name. Passes emit PIX/RenderDoc event markers.

## Design decisions (Phase 0)

| Area | Chosen | Alternatives | Who uses what |
|---|---|---|---|
| Graphics API layer | D3D12 only, behind thin wrappers (`Texture`, `CommandContext`, …) | Multi-API RHI | Multi-API RHIs: Unreal (RHI), Unity (GfxDevice), bgfx, The Forge, Diligent, NVRHI. We have one target, and a fake abstraction would only slow us down. |
| Resource binding | Bindless: SM 6.6 `ResourceDescriptorHeap`, one root signature, indices passed in root constants | Per-pass root signatures plus descriptor tables (classic D3D12) | Classic tables: the D3D12 samples and most pre-2020 engines. Bindless: id Tech 7 (Doom Eternal) and The Forge; Unreal 5 has bindless as an option. Bindless is required for material buffers and GPU-driven draws later. |
| Shader compiler | DXC, SM 6.6, compiled at runtime | FXC (frozen at SM 5.1); Slang; offline compilation | Slang: NVIDIA Falcor. Shipping engines compile offline into caches (Unreal DDC, Unity variants). Runtime compilation now keeps hot reload possible; offline compilation comes in Phase 6. |
| Command recording | `CommandContext`, with state stored on each `Texture` and barriers batched | Raw command lists with hand-written barriers; render graph | Our design is close to Microsoft's MiniEngine (`CommandContext` + `GpuResource` state). The graph (Phase 1) builds on top of it rather than replacing it. |
| Per-frame data | One linear upload allocator, one region per frame slot, guarded by that slot's fence | A ring buffer for each pass; per-draw committed buffers | Linear/ring allocators: MiniEngine `LinearAllocator`, Unreal's dynamic/fast allocators. |
| Depth | Reverse-Z with an infinite far plane, D32F | Standard Z; logarithmic depth | Reverse-Z is standard in Unreal, Unity (D3D/Metal), and Frostbite, and gives the best float precision. |
| Matrix convention | Row-major and row vectors on CPU and GPU (`-Zpr`, `mul(v, M)`) | Column-major plus transposes on upload | Unreal and DirectXMath use row vectors. GL-heritage code uses column vectors. |
| Engine packaging | Static library plus app | Engine as a DLL (hot-reloadable modules) | DLL modules: Unreal editor builds. Monolithic: Godot. A static library is simplest until an editor exists. |
| Error handling | Exceptions for environment and API failures; asserts for bugs | Error codes; exceptions disabled | Most AAA engines (Unreal included) build without exceptions and use checks or asserts. Our exceptions are confined to init and API failures, so switching later is cheap. |

## Known limits (intentional for now)

- **Freed descriptors are reused immediately.** Destroy textures only after a GPU flush, as
  `Resize` and shutdown do. A deferred-release queue comes in Phase 1.
- **Single-threaded recording** on one direct queue.
- **Barriers cover the whole resource**, with no per-subresource or mip tracking.
- **The renderer has no buffer type, mesh path or static upload path yet.** They arrive with
  geometry in Phase 1.
