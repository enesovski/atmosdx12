# Phase 2: scene, assets, and materials

**Status:** ✅ implemented.

**Result:** `scenes/sponza.json` loads Khronos Sponza asynchronously and renders it with:

- base color, normal, metallic-roughness, occlusion and emissive textures
- alpha-tested and double-sided materials
- an ImGui overlay showing frame time, render-graph passes, culling, draw and material counts, and asset-loading
  progress

**Verification:**

- Cold start: 69 textures cooked in about 8 s (Release); the scene is drawable within 1 s, on fallback textures.
- Warm start: everything is resident within 1 s, loaded from the cache.
- GPU validation and resize are clean.
- F5 saves the scene, including across drives.
- Debug and Release builds have no warnings.

**Done-when gap:** the roadmap asks for "many glTF models"; only Sponza was available to test. The scene format, asset
manager and extraction handle any number of entities.

## Module layout added in this phase

```text
scene/      Scene, Entity, Transform, ModelComponent, SceneSerializer (JSON)          graphics-free
assets/     GltfLoader, ModelCooker (.amodel), TextureCooker (DDS), AssetCache        CPU only, thread-safe
runtime/    AssetManager (handles, async, GPU residency), RenderSceneBuilder         ties scene + assets + graphics
ui/         DebugUi (ImGui Win32 + D3D12 backends) implementing IOverlayRenderer
graphics/   Material/MaterialData, GpuScene buckets + material table, UploadQueue (copy queue)
```

## Decisions, alternatives, and who uses what

### Scene model: plain entity structs with optional components (not an ECS yet)

**Alternatives:**

- EnTT or a hand-written archetype ECS: Unity DOTS, Bevy, Flecs-based engines.
- Actor/component object graphs: Unreal `AActor`/`UActorComponent`, Unity `GameObject`/`MonoBehaviour`.

**Why:**

- The only components today are a transform and a model.
- A flat `std::vector<Entity>`, where parents come before children, gives world matrices in one forward pass.
- It serializes trivially.

An ECS pays off with many component types and systems iterating them. Introduce one when gameplay systems appear.
`Scene` is already separate from `RenderScene`, which is the boundary an ECS would sit behind.

### Scene → renderer: rebuild `RenderScene` proxies every frame

**Alternatives:**

- Persistent render proxies updated on change: Unreal `FPrimitiveSceneProxy` in `FScene`.
- Unity's per-frame culling results handed to SRP.

**Why:**

- Rebuilding a flat array is cheap at today's scale and keeps the two sides fully decoupled.
- `RenderSceneBuilder` is the one place to change if this moves to persistent proxies with dirty tracking (with
  GPUScene in Phase 5).

### Asset pipeline: cook on first use, into a content-addressed cache

**Alternatives:**

- An explicit offline cook step: Unreal cooking, Unity's asset import pipeline and Library folder.
- Loading source formats directly every time: Phase 1.

**Why:**

- No tool step to run: the first load cooks, later loads read the cache.
- Cache names hash the source path plus the cooker version, so bumping the version invalidates everything.
- Validity is "cooked file newer than source", the same rule Unity's Library uses in spirit.
- Formats:
  - models: `.amodel`, a flat binary dump of `ModelData`
  - textures: DDS (mips plus block compression), readable by any DDS tool

### Texture compression: BC1 (BC3 with alpha) for color, BC1 for data maps, BC5 for normals

**Alternatives:**

- BC7 everywhere: higher quality.
  - CPU BC7 (DirectXTex) took more than 1 s per 1K texture.
  - GPU BC7 (DirectXTex through a D3D11 device, or AMD Compressonator) is fast but adds a device.
- Basis/KTX2 transcoding: glTF `KHR_texture_basisu`, used by web engines.
- Unreal's default compression settings: BC1/BC3 for color, BC5 for normals.

**Why:** a cold Sponza cook takes about 8 s instead of minutes, at Unreal's default quality. Switching to BC7 with a GPU
compressor later is a one-function change plus a cooker version bump.

### Async loading: thread-pool CPU work; GPU creation and uploads on the main thread via the copy queue

**Alternatives:**

- Creating D3D12 resources on worker threads. The device is free-threaded, but our descriptor heaps and release queue
  would need locks.
- DirectStorage, with GPU decompression and direct file-to-GPU transfers. Forspoken shipped it first.
- Streaming on the graphics queue. Simpler, but it competes with rendering.

**Who uses what:**

- Dedicated copy queues and async upload: Unreal and Unity.
- Load-and-cook on worker threads is universal.

**Why:**

- Workers only touch CPU data, so no GPU object needs a lock.
- `UploadQueue` batches all copies once per frame on the copy queue.
- Resources become usable when its fence passes. Until then, materials point at fallback textures and models are
  simply not drawn.

### Copy-queue state handling: rely on COMMON promotion and decay, with no barriers

**Alternative:** explicit barriers with queue ownership transfers.

**Why:**

- Copy-queue destinations start in COMMON, are promoted to COPY_DEST, and decay back to COMMON.
- On the graphics queue they are promoted to SRV or index-buffer states when first read.
- The CPU waits for the copy fence before any frame references them, so the two queues never overlap on a resource.

### Materials: per-frame material table (StructuredBuffer) plus a material index per draw

**Alternatives:**

- Material constants in a CBV per draw (classic).
- A persistent material buffer updated on change: Unreal's and Unity SRP Batcher's persistent material data.
- Material data packed into each draw record.

**Why:**

- Rewriting only the materials visible this frame (a few hundred bytes for Sponza) into the frame slot's upload buffer
  needs no dirty tracking.
- It is automatically correct when textures become resident mid-stream.
- Each draw stores one `materialIndex`, so materials are shared across draws.

### Alpha modes and culling: pipeline buckets

**Alternatives:**

- One pipeline with dynamic branches on material flags.
- Sorting by full material pipeline keys, which Unreal and Unity do through mesh draw commands and shader variants.

**Why:**

- Four buckets (opaque/masked × single/double-sided) cover glTF.
- Masked geometry alpha-tests only in the depth prepass. Opaque shading then uses `EQUAL`, so `clip` never runs in the
  expensive pass. Unreal and HDRP do the same with masked depth prepasses.
- `BLEND` materials are treated as masked until Phase 3 adds a sorted transparent pass.

### Normal maps: BC5 two-channel, Z rebuilt in the shader

**Alternative:** BC1/BC3 three-channel normals.

**Why:** BC5 gives each axis its own 8-bit-quality block, which Unreal and Unity both use for normal maps. glTF
tangents are imported; when absent, per-triangle UV-gradient tangents are generated. MikkTSpace (Blender, Unreal) would
match authoring tools exactly; add it when a model needs it.

### Debug UI: Dear ImGui drawn as an `IOverlayRenderer`

**Alternatives:**

- A custom retained UI, as Unity's IMGUI/UI Toolkit and Unreal's Slate do for their editors.
- RmlUi or Noesis for game UI.

**Who uses ImGui:** many engine teams for in-game tools; it is ubiquitous.

**Why:**

- Immediate mode is the fastest way to expose stats and settings.
- The renderer only knows the `IOverlayRenderer` interface (dependency inversion), so it never includes ImGui.
- ImGui's D3D12 backend allocates its font SRV from our bindless heap through callbacks.

### Scene files: JSON (nlohmann/json), paths relative to the scene file

**Alternatives:**

- Binary scene formats.
- YAML: Unity's `.unity` files.
- Unreal's binary `.umap` packages.
- USD, for interchange.

**Why:** JSON is human-readable, diffable in git, and trivial to extend. Relative paths keep `scenes/` and `assets/`
portable together. The path falls back to absolute when the model is on another drive.

### Build integration: DirectXTex optimized even in Debug

**Why:** BC compression in an unoptimized build is 10–100× slower. A CMake `block()` scopes `/O2` to the DirectXTex
subdirectory only; the debug CRT stays the same.
