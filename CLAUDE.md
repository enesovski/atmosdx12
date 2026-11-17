# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

AtmosDX12 is a C++20 / Direct3D 12 / HLSL (SM 6.6) render engine in development, aiming for the
scope of Unity's or Unreal's rendering layer. `engine/` builds the `AtmosEngine` static library
and `app/` builds the `AtmosDX12` executable that drives it.

**ROADMAP.md is the plan of record.** Phase 0 (foundation refactor) is done. Phase 1 (render
graph v1 + first geometry) is planned in `docs/phase-1-plan.md`; implement it in that plan's
step order. Follow the roadmap's "done when" criteria, and say so when a task conflicts with it.
ARCHITECTURE.md describes the current structure and records each design decision with its
alternatives. Keep both in sync when the architecture changes.

## Build and run

```bash
cmake -S . -B build -A x64
cmake --build build --config Debug
./build/app/Debug/AtmosDX12.exe
```

- `--config Release` for optimized builds. Debug enables the D3D12 debug layer and DRED, and
  compiles shaders with `-Zi -Od`.
- `AtmosDX12.exe --gpu-validation` turns on GPU-based validation.
- There are no tests or CI. Verify with a warning-free `/W4` build, then run the exe and
  check the console for `D3D12:` messages: debug-layer output is forwarded to the log every
  frame.
- New `.cpp`/`.h` files go into the explicit list in `engine/CMakeLists.txt` (no glob).
  Shaders in `engine/shaders/` are picked up automatically.
- The `AtmosDX12RuntimeFiles` target copies `engine/shaders/` and the Windows SDK's
  `dxcompiler.dll`/`dxil.dll` next to the exe on every build. Shaders compile at startup with
  DXC, so HLSL errors show up as startup exceptions, not build errors. After editing a
  shader, rebuild (to copy it) and restart.

## Architecture

```
app/Application   → graphics/Renderer → graphics/passes/* → graphics/d3d12/*
                       scene/, platform/, core/ are graphics-free and usable from anywhere above
```

`d3d12/` must not include anything from `graphics/` (passes, Renderer, RenderScene). The
app talks to the engine only through `Renderer::Render(RenderScene, RenderView)`,
`Renderer::Resize`, `Camera`, `Window` and `Input`.

**Bindless model.** All pipelines share one root signature (`d3d12/BindlessRootSignature.h`):
- b0: root constants, at most 16 dwords
- b1: `ViewConstants`
- b2: optional per-pass CBV
- s0–s3: static samplers

Textures are never bound. Shaders read `ResourceDescriptorHeap[index]`, with the index from
`Texture::GetSrvIndex()` passed in root constants. `CommandContext::Begin` sets the heap
**before** the root signature, which directly indexed heaps require.

**Writing a pass.** Follow `SkyPass`/`TonemapPass`:
- Build the PSO in the constructor with `PipelineState::CreateGraphics(device, { .debugName,
  .vertexShader, .pixelShader, .renderTargetFormats, ... })`. The defaults suit fullscreen
  passes.
- `Record(context, inputs..., outputs..., settings)` opens a `ScopedGpuEvent`, then calls
  `context.Transition` for inputs, `SetRenderTarget` for outputs, `SetPipeline`,
  `SetRootConstants(struct)` and a draw.
- The Renderer wires which textures each pass reads and writes. Passes never own render
  targets.
- Root-constant structs live in the pass `.cpp`, with a `// Mirrors ...` twin in the HLSL.

**Frame and lifetime rules.**
- `FrameScheduler::BeginFrame` returns a frame slot (0..`FramesInFlight-1`), which is
  separate from the back-buffer index. Its fence guards that slot's command allocator *and*
  its `UploadAllocator` region.
- Per-frame CPU→GPU data goes through `CommandContext::SetRootConstants`, `SetConstantBuffer`
  or `SetGlobalConstantBuffer` (which allocates from the upload ring). Do not create
  per-pass upload buffers.
- Only `CommandContext` changes `Texture` state. Barriers are batched and flushed before
  draw, dispatch or clear.
- Freed descriptors are reused immediately, so destroy or recreate `Texture`s only after
  `FrameScheduler::Flush()`, as `Renderer::Resize` does.
- `Renderer` member declaration order is construction order, and it flushes in its
  destructor.

**Conventions.**
- Left-handed, +Y up, yaw 0 looks along +Z.
- Reverse-Z infinite projection: near = 1, far = 0. Use `GREATER_EQUAL` and clear depth
  to 0.
- Matrices are row-major on the CPU and in HLSL (DXC `-Zpr`), so HLSL uses
  `mul(v, M)` like DirectXMath.
- `ShaderInterop.h` mirrors `shaders/Common.hlsli`, pinned with `static_assert`s; change both
  together.
- Errors:
  - API failures use `ThrowIfFailed(hr, "what")`, which throws `DxException` with the
    HRESULT and the call site.
  - Programmer errors use `ATMOS_ASSERT`.
  - Logging goes through `log::Info/Warning/Error` with `std::format` syntax.
- Every engine symbol lives in `namespace atmos`, and includes are rooted at `engine/`
  (`#include "graphics/d3d12/Texture.h"`).
- Give every D3D12 object a debug name (`SetDebugName`).
