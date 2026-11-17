# Phase 1 plan: render graph v1 and first geometry

**Status:** planned, not implemented.

**Goal:** render real 3D meshes, with every pass recorded through a render graph.

**Done when:**

- A glTF scene (Sponza) renders with correct depth, a lambert + sun forward pass, and the sky only where no geometry
  is drawn.
- `Renderer::Render` only builds and executes a graph; no barrier, render-target, or resize logic remains in it.
- A pass whose output nobody reads is culled, which a debug graph dump shows.
- Resize works with no hand-written resize code for transient targets.
- `--gpu-validation` and the debug layer report nothing.
- With no scene argument, the app still runs and shows a built-in test mesh.

**Frame after Phase 1:**

```text
import BackBuffer
create Depth (D32F), SceneColor (RGBA16F)       ← transient, pooled by the graph

DepthPrepass   writes Depth (clear 0)
Opaque         reads Depth (equal test), writes SceneColor (clear)
Sky            reads Depth (far-plane test), writes SceneColor (load)
Tonemap        reads SceneColor, writes BackBuffer → PRESENT
```

---

## Steps

Each step is one commit that builds, runs, and keeps validation clean.

### S1. Engine plumbing carried over from Phase 0

- `core/FrameTimer`: a `steady_clock` delta, clamped to 0.1 s so a breakpoint doesn't make the camera jump.
- `Input` gains key states, fed from `WM_KEYDOWN`/`WM_KEYUP` and cleared on `WM_KILLFOCUS`.
- `app/FlyCameraController`:
  - WASD to move, Q/E for down/up, Shift for speed, right mouse button to look
  - uses `Camera::SetPosition`, `GetForward`, and a new `GetRight`
  - lives in the app because camera control is gameplay logic, not engine logic
- **Third-party code:** CMake `FetchContent` pinned to a release tag (see decision D7). The first dependency is
  cgltf.

### S2. `Buffer`, static uploads, and deferred release

- **`Buffer` + `BufferDesc`:**
  - `{ size, stride (0 = raw), usage flags (ShaderResource, UnorderedAccess, Index), heap (Default | Upload), debugName }`
  - persistent SRV/UAV for bindless access (`GetSrvIndex()`)
  - `GetGpuAddress()` for index buffers and root CBVs
  - tracked state, same as `Texture`
- **`UploadContext`, for load-time uploads only:**
  - its own command allocator and list on the graphics queue
  - staging memory in an upload buffer
  - `CopyBufferRegion`, then a transition to the final state
  - `Flush()` waits on its own fence
- **`DeferredReleaseQueue`, owned by `GraphicsDevice`:**
  - Destroying a `Texture` or `Buffer` enqueues its `ComPtr` and descriptors, tagged with the current frame's fence
    value.
  - `FrameScheduler::BeginFrame` releases everything the GPU has finished with.
  - This removes the "only destroy after Flush" rule. Swap-chain resize still flushes, because DXGI requires it.

### S3. Render graph v1

New folder `engine/graphics/rendergraph/`.

```cpp
struct SceneColorPassData { RGTexture sceneColor; RGTexture depth; };

RGTexture depth = graph.CreateTexture({ .width = w, .height = h, .format = DepthFormat, .name = L"Depth" });

graph.AddPass<SceneColorPassData>(L"Sky",
    [&](RGBuilder& builder, SceneColorPassData& data) {
        data.sceneColor = builder.WriteColor(sceneColor, LoadOp::Load);
        data.depth = builder.ReadDepth(depth);
    },
    [=](const SceneColorPassData& data, RGContext& context) {
        skyPass.Draw(context.commands, sky);
    });
```

The graph is built, compiled, and executed every frame, then reset. Its memory is reused across frames.

- **`RenderGraph`:**
  - `CreateTexture(desc)` for transients
  - `ImportTexture(Texture&, finalState)` for the back buffer and future history textures
  - `AddPass`, `Compile`, `Execute(CommandContext&)`
  - `DumpToLog()`
- **`RGBuilder`, which declares how a pass uses each resource:**
  - `ReadTexture` (SRV)
  - `WriteColor(handle, LoadOp)` and `WriteDepth(handle, LoadOp)`
  - `ReadDepth` (read-only DSV)
  - `ReadWriteTexture` (UAV)
  - `SetSideEffect()`, which stops the pass from being culled
- **`Compile`:**
  - Finds each resource's producer passes.
  - Culls passes whose outputs nobody consumes, starting from imported resources and side-effect passes.
  - Computes each transient's first and last use.
- **`Execute`, for each live pass in declaration order:**
  - Gets or creates its transients from the `TexturePool`.
  - Calls `CommandContext::Transition` for each declared access.
  - Binds the declared color and depth targets, applying their load ops: clear or load.
  - Emits a PIX event and runs the execute lambda.
  - After the last pass, transitions imported resources to their final state.
- **`TexturePool`:**
  - Reuses `Texture`s by `TextureDesc` (size, format, usage).
  - Evicts entries unused for N frames through the deferred-release queue.
  - Resize then works on its own: the next frame asks for a different size.
- **Passes keep owning their PSOs** but stop binding targets. `SkyPass::Draw(commands, sky)` only sets the
  pipeline, the root constants, and draws.
- **Port Sky and Tonemap first** and check that the output matches today's pixel for pixel before adding
  geometry. Delete `Renderer::CreateRenderTargets` and `hdrTarget_`.

### S4. Meshes and glTF loading

- **`Mesh`:**
  - `positions` buffer (float3, raw)
  - `attributes` buffer (normal float3 + uv0 float2, 20 B/vertex)
  - `indices` (uint32)
  - `submeshes { indexOffset, indexCount, baseColor }`
  - local AABB
  - tangents and textures come in Phase 2
- **`GltfLoader` (cgltf):**
  - Loads `.gltf`/`.glb`.
  - Flattens the node hierarchy into `{ meshIndex, worldMatrix }` instances.
  - Reads only `baseColorFactor` from materials.
  - Converts glTF's right-handed coordinates into the engine's left-handed space by negating Z on positions and
    normals and flipping winding.
- **`Mesh::CreateCube()`** as a built-in fallback, so the app runs without assets.
- **Assets:** `assets/` is git-ignored. The README points to Sponza in Khronos glTF-Sample-Assets. Pass a scene with
  `AtmosDX12.exe --scene path/to/Sponza.gltf`.

### S5. Render scene, per-frame instance data, and CPU culling

- `RenderScene` gains `std::vector<MeshInstance> { const Mesh*, XMFLOAT4X4 world, worldAabb }`. This is the minimal
  form of the render-proxy array that Phase 2 builds properly.
- **`InstanceData` per draw:**
  - `{ float4x4 world; float4x4 normalMatrix; float4 baseColor; }`
  - written each frame into a per-frame-slot upload-heap structured `Buffer` with a persistent SRV
  - one buffer per slot, protected by the same fence as `UploadAllocator`
- **Root constants per draw:** `{ instanceIndex, positionBufferIndex, attributeBufferIndex }`, 3 of 16 dwords.
- **CPU frustum culling:**
  - five planes extracted from `viewProjection`; the infinite reverse-Z far plane is skipped
  - tested against world AABBs
  - a flat loop over a contiguous array
  - visible and culled counts logged in the graph dump

### S6. Geometry passes

- **`Mesh.hlsl` (vertex pulling):**
  - The VS loads `positions[SV_VertexID]` and `attributes[SV_VertexID]` through
    `ResourceDescriptorHeap[...]` `ByteAddressBuffer`s.
  - Index buffers still use `IASetIndexBuffer`.
  - `DepthPrepass` and `Opaque` share one `precise` position transform, so their depths match exactly.
- **`DepthPrepassPass`:** a position-only VS, no PS, `DepthMode::ReadWrite`, back-face culling.
- **`OpaquePass`:** `DepthMode::ReadOnly` with `EQUAL`, so each pixel is shaded once. Temporary lighting:
  `baseColor * (ambient + saturate(dot(N, sunDir)) * sunColor)`. Phase 3 replaces it with deferred + clustered PBR.
- **`SkyPass`** moves after Opaque. Its triangle sits at z = 0 with a `GREATER_EQUAL` read-only test, so it passes
  only where depth is still cleared to 0. The sky is shaded only where no geometry is.
- `PipelineState` gains `DepthMode::ReadOnlyEqual`, or a depth-func field.
- **`CommandContext`** gains `SetIndexBuffer(Buffer&)` and `DrawIndexed(indexCount, startIndex)`.

### S7. Verification and docs

- Pixel-compare the sky with no geometry against Phase 0.
- Run Sponza with `--gpu-validation`.
- Test resize and minimize.
- Check the graph dump, including a disabled debug pass appearing as culled.
- Update ARCHITECTURE.md, adding a render-graph section and the decisions below, and update CLAUDE.md.

---

## Decisions, alternatives, and who uses what

### D1. Render graph style: per-frame immediate graph with setup and execute lambdas

| Option | How it works | Used by |
|---|---|---|
| **Per-frame graph, setup + execute lambdas (chosen)** | Rebuilt every frame. Passes declare resource use in setup and record commands in execute. | Frostbite FrameGraph (GDC 2017), Unity RenderGraph (HDRP; URP since Unity 6), Google Filament FrameGraph |
| Per-frame graph with reflected parameter structs | Same idea, but resources are declared through macro-generated shader-parameter structs. | Unreal RDG (`FRDGBuilder::AddPass` + `BEGIN_SHADER_PARAMETER_STRUCT`) |
| Retained, data-driven graph | Built once from config or scripts and changed rarely. | Ogre-Next Compositor, Bevy render graph |
| Command-level automatic sync | No passes; the device layer records commands and infers barriers and order. | Godot 4.3+ `RenderingDeviceGraph` |
| Manual ordering and barriers | What Phase 0 has. | MiniEngine, most hobby engines |

**Why:** a per-frame graph makes feature toggles trivial: if a pass isn't added, its resources are never created. It
is also what the big engines converged on. We use lambdas instead of Unreal-style reflection macros because they
need no codegen or macros and stay readable. Unreal's approach pays off when tooling validates shader parameters
automatically, which we don't need yet. v1 runs passes in declaration order with no reordering, which is also how
RDG behaves outside async compute.

### D2. Barriers: derived by the graph, executed through the existing `CommandContext`

**Alternatives:**

- The graph computes a full barrier batch per pass (split begin and end barriers).
- Enhanced Barriers through the Agility SDK.

**Why:** `Texture` already tracks its state, so v1 only needs to call `Transition` before each pass. The graph owns
when transitions happen, and the context owns how. Split and enhanced barriers come in Phase 5 (v2), where they pay
off with async compute.

### D3. Transient memory: a pool of committed textures, without aliasing

**Alternatives:**

- Placed resources aliased in shared heaps, using lifetime intervals. Used by Frostbite, Unreal (RDG transient
  allocator), and Unity HDRP.
- Persistent members per target, which is today's approach.

**Why:** the pool gives automatic resize and creation now. Aliasing saves memory, but it needs placed resources,
aliasing barriers, and careful debugging, which is v2 work. The pool's API won't change when aliasing arrives.

### D4. Vertex input: vertex pulling from bindless buffers, with a separate position stream

**Alternatives:**

- Input-assembler vertex layouts. Used by the D3D12 samples, Unity, and Unreal vertex factories.
- One interleaved stream.

**Who uses pulling:**

- Unreal Nanite fetches from its own cluster data.
- GPU-driven pipelines fetch from buffers, e.g. Assassin's Creed Unity (Haar & Aaltonen, SIGGRAPH 2015).

**Who splits streams:** Unreal keeps positions separate (`FPositionVertexBuffer`) from tangents and UVs.

**Why:**

- Our root signature has no input-layout flag, and pulling fits the bindless model.
- Pulling removes per-mesh vertex-format plumbing.
- Pulling leads straight into GPU-driven rendering and meshlets in Phase 5.
- A position-only stream makes the depth prepass and shadow passes read less memory.

### D5. Depth prepass, then opaque with an `EQUAL` test

**Alternatives:**

- No prepass, relying on front-to-back sorting and early-Z.
- A visibility buffer: Burns & Hunt 2013; Nanite; The Forge.

**Who uses a prepass:**

- Unity URP makes it optional ("depth priming"). Unity HDRP always does one.
- Unreal controls it with `r.EarlyZPass`.
- Forward-clustered renderers such as Doom (2016) use one.

**Why:**

- Forward shading with no overdraw.
- Phase 3+ needs the depth buffer early anyway, for clustered light culling, SSAO, and Hi-Z occlusion.
- A sky test against cleared depth becomes free.

### D6. Shading in Phase 1: temporary forward lambert (and why not deferred yet)

The real shading decision belongs to Phase 3, when there are lights to shade. Phase 1 needs only enough lighting to
check normals and depth. The Phase 3 options are:

| Path | Used by | Strength | Weakness |
|---|---|---|---|
| Forward | Unity built-in forward, mobile renderers | Simple, MSAA, any material | Cost grows with lights per object |
| **Forward+ / clustered forward** | Doom (2016) / Doom Eternal, Godot 4 Forward+, Unity URP Forward+ | Many lights, MSAA, transparents share the path | Every material evaluates all lighting; overdraw needs a prepass |
| **Deferred + clustered/tiled lighting (planned)** | Unreal (default), Unity HDRP (default), Frostbite, CryEngine | Lighting decoupled from geometry; easy screen-space effects | GBuffer bandwidth; no MSAA; transparents need a forward path |
| Visibility buffer | Unreal Nanite, The Forge | Lowest bandwidth; ideal for dense GPU-driven geometry | Complex material evaluation; built on GPU-driven rendering |

The plan stays **deferred + clustered, with forward for transparents** (Phase 3), matching Unreal and HDRP. Revisit a
visibility buffer once GPU-driven rendering exists (Phase 5).

### D7. Third-party code: CMake `FetchContent`, pinned

**Alternatives:**

- vcpkg manifest: binary caching and many ports, but needs installing (it isn't installed here).
- Conan.
- Git submodules.
- Vendored sources: Godot keeps them in `thirdparty/`; Unreal keeps prebuilt `ThirdParty/` libraries.

**Why:**

- No extra tool to install.
- Pinned versions with reproducible builds.
- Fine for small source libraries (cgltf, ImGui, DirectXTex).

Revisit vcpkg if dependency count or build times grow.

### D8. glTF loader: cgltf

**Alternatives:**

- fastgltf: C++17 and SIMD-fast; used by the vk-guide tutorial.
- tinygltf: used by the Sascha Willems Vulkan samples.
- Assimp: many formats, but heavy.

**Who imports into their own formats:** Unity (FBX SDK, glTFast), Unreal (Interchange), Godot (its own glTF importer,
ufbx for FBX).

**Why:**

- One C file with no dependencies, proven in Google Filament's `gltfio` and raylib.
- We convert into our own `Mesh` right away, so loader speed matters little until Phase 2 adds a cooked format.

### D9. Per-draw data: a per-frame instance structured buffer, with indices in root constants

**Alternatives:**

- A CBV per draw (classic).
- Matrices in root constants: too large for 16 dwords.
- A persistent GPU scene buffer, updated by deltas: Unreal GPUScene; Unity BatchRendererGroup / SRP Batcher keep
  per-object data resident.

**Why:** this is one upload per frame and one index per draw, and it is the shape GPUScene grows from in Phase 5. A
persistent, delta-updated version isn't worth it before there are many static objects.

### D10. Culling: brute-force CPU frustum test over flat arrays

**Alternatives:**

- Spatial hierarchies (BVH or octree). Godot 4's scene culling uses a dynamic BVH.
- GPU culling. Unreal and GPU-driven engines; Phase 5 here.

**Who brute-forces:** Unreal frustum-culls packed primitive bounds in parallel loops; Unity's SRP culling runs in
native jobs.

**Why:** a linear, cache-friendly loop handles tens of thousands of objects in well under a millisecond, and needs no
structure to maintain.

### D11. Static uploads: synchronous on the graphics queue at load time

**Alternatives:**

- An async copy queue with streaming. Every shipping engine does this; here it comes in Phase 2.
- DirectStorage (GPU decompression; Forspoken was the first PC game to ship with it).

**Why:** loading happens once at startup in Phase 1. A blocking upload is correct and simple, and the `UploadContext`
API stays the same when it moves to a copy queue.

---

## Risks and open questions

- **glTF handedness:** negating Z must also flip triangle winding, or back-face culling removes the wrong side. Check
  it with the cube and with Sponza.
- **Depth-equal precision:** the prepass and the opaque VS must compute positions identically. Use one shared
  function and `precise`.
- **Pool eviction versus in-flight frames:** evicted textures go through the deferred-release queue, never straight
  to `Release()`.
- **Large scenes and the 4 MB upload region:** Sponza's instance count is small, but raise `UploadBytesPerFrame` or
  size the instance buffer separately if the log reports exhaustion.
