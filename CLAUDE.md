# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

AtmosDX12 is a C++20 / Direct3D 12 / HLSL (SM 6.6) render engine in development, aiming for the scope of Unity's or
Unreal's rendering layer. `engine/` builds the `AtmosEngine` static library and `app/` builds the `AtmosDX12`
executable that drives it.

**ROADMAP.md is the plan of record.** Phases 0 and 1 are done; Phase 2 (scene, assets, materials) is next. Follow
the roadmap's order and its "done when" criteria, and say so when a task conflicts with it. ARCHITECTURE.md
describes the current structure and records each design decision with its alternatives. Keep both in sync when the
architecture changes.

## Build and run

```bash
cmake -S . -B build -A x64
cmake --build build --config Debug
./build/app/Debug/AtmosDX12.exe
```

**Command-line options:**

- `--scene path/to/file.gltf` loads a glTF instead of the built-in test scene.
- `--dump-frame` logs the first frame's render graph (live and culled passes) and culling stats.
- `--gpu-validation` turns on GPU-based validation.

**Build notes:**

- `--config Release` gives optimized builds. Debug enables the D3D12 debug layer and DRED, and compiles shaders with
  `-Zi -Od`.
- **No tests or CI.** Verify with a warning-free `/W4` build, then run the exe and check the console for `D3D12:`
  messages; debug-layer output is forwarded to the log every frame.
- **New `.cpp`/`.h` files** go into the explicit list in `engine/CMakeLists.txt` (no glob). Shaders in
  `engine/shaders/` are picked up automatically.
- **Third-party code** comes through CMake `FetchContent`, pinned to a tag. cgltf is the only dependency so far, and
  configuring needs network access the first time.
- **Shaders:** `AtmosDX12RuntimeFiles` copies `engine/shaders/` and the SDK's `dxcompiler.dll`/`dxil.dll` next to the
  exe on every build. Shaders compile at startup, so HLSL errors are startup exceptions. After editing a shader,
  rebuild (to copy it) and restart.
- **Assets:** `assets/` is git-ignored. Put test models such as Khronos Sponza there.

## Architecture

```
app/ → graphics/Renderer → graphics/passes/* → graphics/rendergraph/* → graphics/d3d12/*
assets/ produces CPU MeshData; scene/, platform/, core/ are graphics-free
```

- **Layering:** `d3d12/` must not include `graphics/` code above it. `rendergraph/` must not know specific passes.
  `assets/` must not touch the GPU.
- **App boundary:** the app uses `Renderer` (`Render`, `Resize`, `CreateMeshes`, `RequestFrameDump`), `RenderScene`,
  `RenderView`, `Camera`, `Window`, `Input`, and the loaders.

**Bindless model.** All pipelines share one root signature (`d3d12/BindlessRootSignature.h`):

- b0: root constants, at most 16 dwords
- b1: `ViewConstants`
- b2: optional per-pass CBV
- s0–s3: static samplers

Textures and buffers are never bound. Shaders read `ResourceDescriptorHeap[index]`, with indices passed in root
constants. Mesh vertices are pulled from `ByteAddressBuffer`s; there are no input layouts.

**Writing a pass.** Follow `SkyPass`/`OpaquePass`:

- **Constructor:** builds the PSO with `PipelineState::CreateGraphics(device, { .debugName, .vertexShader, ... })`.
  The designated-initializer fields must stay in declaration order.
- **Interface:** `AddToGraph(RenderGraph&, inputs..., RGTexture handles...) const` adds one graph pass.
- **Setup lambda:** only declares accesses (`ReadTexture`, `WriteColor(LoadOp)`, `WriteDepth(LoadOp)`, `ReadDepth`,
  `ReadWriteTexture`).
- **Execute lambda:** only records commands (`SetPipeline`, root constants, draws). It captures handles and data by
  value, and `this` for the PSO.
- **Never bind targets, transition, or clear inside a pass**; the graph does that from the declarations.
- **New render targets:** create them with `graph.CreateTexture(desc)` in `Renderer::BuildFrameGraph`. Don't add them
  as members. Import only external textures.
- **Mirrored structs:** root-constant/CBV structs live in the pass `.cpp`, with a `// Mirrors ...` twin in the HLSL.
- **Mesh passes:** call `GpuScene::RecordDraws(context.commands)` after setting their pipeline.

**Frame and lifetime rules.**

- `FrameScheduler::BeginFrame` returns a frame slot (0..`FramesInFlight-1`), separate from the back-buffer index.
  Its fence guards that slot's allocator, `UploadAllocator` region, and `GpuScene` draw buffer.
- Per-frame constants go through `CommandContext::SetRootConstants`, `SetConstantBuffer`, or
  `SetGlobalConstantBuffer`.
- Destroying a `Texture`/`Buffer` is always safe: it goes through the device's `DeferredReleaseQueue`. Only
  swap-chain resize needs a flush plus `ReleaseAll()`.
- Only `CommandContext` changes `Texture` state. Static `Buffer`s stay in COMMON and rely on implicit
  promotion/decay.
- Load-time uploads go through `UploadContext` (via `Renderer::CreateMeshes`), which blocks until done.

**Conventions.**

- **Coordinates:** left-handed, +Y up, yaw 0 looks along +Z. Front faces are **clockwise** (`CullMode::Back`).
- **glTF import** mirrors Z and reverses winding (see `GltfLoader.cpp`).
- **Depth:** reverse-Z infinite projection, near = 1, far = 0. Use `GREATER_EQUAL` (`EQUAL` after the prepass) and
  clear to 0.
- **Matrices:** row-major with row vectors on CPU and GPU (DXC `-Zpr`, `mul(v, M)`).
- **Mirrored types:** `ShaderInterop.h`/`Common.hlsli`, `GpuScene.h`/`Mesh.hlsl`, and `MeshData.h`'s
  `VertexAttributes` are mirrored types. Change both sides together.
- **Errors:** `ThrowIfFailed(hr, "what")` throws `DxException` with the HRESULT and call site. `ATMOS_ASSERT` is for
  programmer errors. Logging uses `log::Info/Warning/Error` with `std::format` syntax.
- **Namespaces and includes:** engine code lives in `namespace atmos`, and includes are rooted at `engine/`. Give
  every D3D12 object a debug name.
