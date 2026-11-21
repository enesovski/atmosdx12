# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

AtmosDX12 is a C++20 / Direct3D 12 / HLSL (SM 6.6) render engine in development, aiming for the scope of Unity's or
Unreal's rendering layer. `engine/` builds the `AtmosEngine` static library, `editor/` builds the `AtmosEditor`
library (a docked ImGui editor like Unity's or Unreal's), and `app/` builds the `AtmosDX12` executable that runs it.

**ROADMAP.md is the plan of record.** Phases 0-4 are done; Phase 5 (image quality: motion vectors, TAA, HDR
post) is next. The focus is rendering quality on static scenes (Sponza): reflections, GI and ray
tracing. Animation, particles and terrain are out of scope. Each phase's
decisions are recorded in `docs/phase-N-plan.md`. Follow
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

- With no arguments it opens the last scene, else `scenes/sponza.json` (the project root is found from the working
  directory or above the exe).
- `--scene file` opens a `.json` scene or a single `.gltf`/`.glb`.
- **In the editor:** RMB fly (WASD/QE, Shift, wheel = speed), Alt+LMB orbit, MMB pan, wheel dolly, F focus;
  Q/W/E/R tools; Ctrl+Z/Y undo/redo; Ctrl+S (or F5) saves, including the current camera as the start view;
  F11 or F1 fullscreen viewport. Help > Controls lists everything.
- `--dump-frame` logs the first frame's render graph (live and culled passes) and culling stats.
- `--gpu-validation` turns on GPU-based validation.
- `--spawn-lights N` adds N random point lights.

**Build notes:**

- `--config Release` gives optimized builds. Debug enables the D3D12 debug layer and DRED, and compiles shaders with
  `-Zi -Od`.
- **No tests or CI.** Verify with a warning-free `/W4` build, then run the exe and check the console for `D3D12:`
  messages; debug-layer output is forwarded to the log every frame.
- **New `.cpp`/`.h` files** go into the explicit list in `engine/CMakeLists.txt` or `editor/CMakeLists.txt` (no
  glob). Shaders in `engine/shaders/` are picked up automatically.
- **Third-party code** comes through CMake `FetchContent`, pinned to a tag or commit: cgltf, nlohmann/json and
  DirectXTex in `engine/ThirdParty.cmake`; Dear ImGui (docking tag) and ImGuizmo in `editor/ThirdParty.cmake`. The first configure needs network access. DirectXTex is built with `/O2`
  even in Debug, because BC compression is unusably slow otherwise.
- **Shader layout:** `Common.hlsli` (view constants, samplers), `Scene/` (draw and material loading, GBuffer
  encoding), `Lighting/` (BRDF, clusters, shadows, IBL, and `SurfaceLighting.hlsli`, the one lighting function
  shared by deferred and transparent), `Sky/`, and `Passes/` (one entry-point file per pass).
- **Shaders:** `AtmosDX12RuntimeFiles` copies `engine/shaders/` and the SDK's `dxcompiler.dll`/`dxil.dll` next to the
  exe on every build. Shaders compile at startup, so HLSL errors are startup exceptions. After editing a shader,
  rebuild (to copy it) and restart.
- **Assets:** `assets/` is git-ignored; Sponza is expected at
  `assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf`. Scenes live in `scenes/` (tracked).
- **Asset cache:** the first load cooks models (`.amodel`) and textures (`.dds`) into `<exe dir>/AssetCache/`.
  After changing a cooker, bump `FormatVersion` (`ModelCooker.cpp`) or `CookerVersion` (`TextureCooker.cpp`).

## Architecture

```
app/ → editor/ (EditorApp, panels, ImGui) → runtime/ (AssetManager, RenderSceneBuilder) → graphics/Renderer
       → passes/ → rendergraph/ → d3d12/
                 ↘ scene/ (graphics-free)   ↘ assets/ (CPU-only cooking)
```

- **Layering:**
  - `d3d12/` must not include `graphics/` code above it.
  - `rendergraph/` must not know specific passes.
  - `assets/` must not touch the GPU, and must stay thread-safe, because it runs on workers.
  - `scene/` stores asset handles, never GPU types.
  - Only `runtime/` combines scene + assets + graphics.
  - **The engine never includes ImGui**; only `editor/` does. The renderer sees the editor UI as an
    `IOverlayRenderer`, and editor visualization it must draw comes as data (`RenderScene::overlays`).
- **Editor rules:**
  - Panels derive from `editor/panels/Panel` and read/edit through `EditorContext`; they own no scene state.
  - Edit the `Scene` directly for live feedback, then call `SceneDocument::MarkEdited("Label")`; the edit becomes
    one undo step (a scene snapshot) when no widget or gizmo is active. Structural edits go through
    `SceneDocument` (create, delete, duplicate, reparent) because they renumber entities.
  - Actions that may need a prompt, a dialog or the window go through `EditorContext::requests`.
  - Colors come from `EditorTheme` tokens (`GetThemeColors()`), never literals; icons from `style/Icons.h`;
    property rows from `PropertyGrid` (reset arrows, units, tooltips).
  - A new view mode is one entry in `editor/ViewModes.cpp`; a new render setting is a `RenderSettings` field,
    its serializer key, and a row in `RenderSettingsPanel`.
  - Editor state lives in `Saved/Editor/` (git-ignored): `Layout.ini` and `EditorPrefs.json`.
- **Assets at runtime:** request with `AssetManager::LoadModel` (async). Call `AssetManager::Update()` once per frame
  on the main thread. `GetModel()` returns nullptr until the model is ready. GPU resources are created only on the
  main thread, and uploads go through `UploadQueue` (copy queue).

**Bindless model.** All pipelines share one root signature (`d3d12/BindlessRootSignature.h`):

- b0: root constants, at most 16 dwords
- b1: `ViewConstants`
- b2: optional per-pass CBV (`LightingConstants` for lighting passes)
- s0–s4: static samplers (s4 is the shadow comparison sampler)

Textures and buffers are never bound. Shaders read `ResourceDescriptorHeap[index]`, with indices passed in root
constants. Mesh vertices are pulled from `ByteAddressBuffer`s; there are no input layouts.

**Writing a pass.** Follow `SkyPass`/`GBufferPass`:

- **Constructor:** builds the PSO with `PipelineState::CreateGraphics(device, { .debugName, .vertexShader, ... })`.
  The designated-initializer fields must stay in declaration order.
- **Interface:** `AddToGraph(RenderGraph&, inputs..., RGTexture handles...) const` adds one graph pass.
- **Setup lambda:** only declares accesses (`ReadTexture`, `WriteColor(LoadOp)`, `WriteDepth(LoadOp, slice)`,
  `ReadDepth`, `ReadWriteTexture(mip)`, `ReadBuffer`, `ReadWriteBuffer`).
- **Execute lambda:** only records commands (`SetPipeline`, root constants, draws). It captures handles and data by
  value, and `this` for the PSO.
- **Never bind targets, transition, or clear inside a pass**; the graph does that from the declarations.
- **New render targets and buffers:** create them with `graph.CreateTexture`/`CreateBuffer`, either in
  `SceneRenderer::AddPasses` or inside the pass that owns the output (which returns the handles). Don't add them as
  members. Import only persistent resources (back buffer, `SkyLight`'s cubes).
- **Where passes go:** `SceneRenderer` owns the passes and the frame's pass order; `Renderer` only runs the frame
  loop.
- **Mirrored structs:** root-constant/CBV structs live in the pass `.cpp`, with a `// Mirrors ...` twin in the HLSL.
- **Mesh passes:** set a pipeline per `DrawBucket`, then call `GpuScene::RecordDraws(commands, drawList, bucket,
  viewBufferIndex, viewIndex)` with a `DrawList` culled for that view. Masked buckets alpha-test only in the depth
  prepass; later passes use `EQUAL` depth.

**Frame and lifetime rules.**

- `FrameScheduler::BeginFrame` returns a frame slot (0..`FramesInFlight-1`), separate from the back-buffer index.
  Its fence guards that slot's allocator, `UploadAllocator` region, and every `FrameStructuredBuffer` (draws,
  materials, views, lights).
- Per-frame constants go through `CommandContext::SetRootConstants`, `SetConstantBuffer`, or
  `SetGlobalConstantBuffer`.
- Destroying a `Texture`/`Buffer` is always safe: it goes through the device's `DeferredReleaseQueue`. Only
  swap-chain resize needs a flush plus `ReleaseAll()`.
- Only `CommandContext` changes `Texture` state. Static `Buffer`s stay in COMMON and rely on implicit
  promotion/decay.
- **Copy-queue uploads:** a resource may be referenced by a frame only after `UploadQueue::IsComplete(fence)`; the
  `AssetManager` enforces this. Copy destinations stay in COMMON.

**Conventions.**

- **Coordinates:** left-handed, +Y up, yaw 0 looks along +Z. Front faces are **clockwise** (`CullMode::Back`).
- **glTF import** mirrors Z (positions, normals, tangents with W negated, node matrices) and reverses winding (see
  `GltfLoader.cpp`).
- **Depth:** reverse-Z infinite projection, near = 1, far = 0. Use `GREATER_EQUAL` (`EQUAL` after the prepass) and
  clear to 0.
- **Matrices:** row-major with row vectors on CPU and GPU (DXC `-Zpr`, `mul(v, M)`).
- **Mirrored types:** `ShaderInterop.h`/`Common.hlsli` + `Scene/ViewData.hlsli`, `GpuScene.h` +
  `Material.h`/`Scene/SceneData.hlsli`, `lighting/LightingTypes.h`/`Lighting/LightingCommon.hlsli`,
  `RenderSettings.h`'s `DebugView`, and `MeshData.h`'s `VertexAttributes` are mirrored types. Change both sides together, and bump the model
  `FormatVersion` when the vertex layout changes.
- **Errors:** `ThrowIfFailed(hr, "what")` throws `DxException` with the HRESULT and call site. `ATMOS_ASSERT` is for
  programmer errors. Logging uses `log::Info/Warning/Error` with `std::format` syntax.
- **Namespaces and includes:** engine code lives in `namespace atmos`, and includes are rooted at `engine/`. Give
  every D3D12 object a debug name.
