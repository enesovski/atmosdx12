# AtmosDX12 architecture

AtmosDX12 is a specialized atmospheric renderer. The current frame contains one
sky pass; geometry, HDR, volumetric fog/clouds, and post-processing remain future
work. There is no RHI, render graph, ECS, material system, or pass registry.

## Ownership and data flow

`Application` owns input, the Win32 `Window`, `Camera`, a `RenderView`, CPU
`SkySettings`, and `Renderer`. The window borrows input, which outlives it. Each
loop pumps events, updates the camera, copies its basis into the view, and invokes
`Renderer::Render(view, skySettings)`. Application knows no GPU constant layout or
D3D12 commands. The control UI and simulation clock have been removed. Minimizing
suspends rendering with `WaitMessage`.

`RenderView` is camera basis/projection plus viewport dimensions. It reuses the
existing `CameraBasis` value and contains no environment or GPU-layout data. The
caller supplies positive viewport dimensions matching the render target; the
current window/swapchain remains fixed at 1280x720. Resize support is outside the
current implementation. `Camera` remains independent of D3D12.

`SkySettings` contains only sun azimuth/elevation in degrees and intensity.
`CalculateSunDirection` returns a unit vector with +Y up and azimuth measured from
+Z toward +X, matching camera yaw. Defaults approximate the former initial sun
position. `SkyPass` converts these settings into shader constants. Twilight colors
depend on sun height; no time simulation is involved.

`Renderer` owns these concrete components in dependency order:

- `D3D12Context`: DXGI factory, hardware adapter, device, and direct queue.
- `SwapChain`: two RGBA8 flip-discard buffers and their RTV heap; vsynced present.
- `FrameScheduler`: two command allocators, one command list, fence, and event.
  Its queue pointer borrows the context's longer-lived queue.
- `SkyPass`: its graphics pipeline and a persistently mapped `UploadBuffer` with
  two 256-byte-aligned constant slots.

`Renderer` prepares render targets and invokes passes in explicit order. It does
not calculate sky lighting or pack sky constants. `SkyPass` owns only sky pipeline
configuration, GPU constants, and draw commands; it has no stored viewport size,
input, UI, presentation, or synchronization responsibility.

## Source organization

```text
src/
  Application.h/.cpp
  main.cpp
  input/Input.h
  platform/Window.h/.cpp
  scene/Camera.h/.cpp
  scene/SkySettings.h
  graphics/
    RenderView.h
    Renderer.h/.cpp
    passes/SkyPass.h/.cpp
    d3d12/
      D3D12Context.h/.cpp
      SwapChain.h/.cpp
      FrameScheduler.h/.cpp
      UploadBuffer.h
      GraphicsConfig.h
      DxError.h
      GraphicsPipeline.h/.cpp
      ShaderCompiler.h/.cpp
shaders/Sky.hlsl
```

Dependencies flow from application coordination to rendering, then to passes and
D3D12 infrastructure. Scene camera/settings are CPU-side values. D3D12
infrastructure has no dependency on Application, Renderer, passes, or scene data.

`GraphicsPipeline` creates and owns a root signature and PSO and binds them. It
accepts native `D3D12_ROOT_SIGNATURE_DESC` and `D3D12_GRAPHICS_PIPELINE_STATE_DESC`
values, so shader bytecode, input layout, depth, blending, culling, and render
target formats remain explicit caller choices. It supplies its owned root
signature to the PSO description. Descriptor pointers/bytecode only need to remain
valid during construction; no descriptors or compiler blobs are retained.
`SkyPass` configures its fullscreen, opaque, unculled, depth-disabled pipeline and
owns it directly by value.

`ShaderCompiler::Compile` is a small utility around `D3DCompileFromFile`, preserving
standard include handling, strictness, diagnostics, and Debug flags. SkyPass
selects VSMain/vs_5_1 and PSMain/ps_5_1. Renderer resolves the shader path relative
to the executable, independent of the working directory. CMake copies HLSL;
compilation happens at startup rather than during the C++ build.

## Frame and resource lifetime

The explicit frame lifecycle in `Renderer::Render` is:

1. `BeginFrame` waits for the selected scheduler slot's previous fence before
   resetting its allocator/list. This also protects that slot's constants.
2. Fetch the current DXGI back buffer independently of the scheduler slot.
3. Transition present to render target; clear and set viewport, scissor, and RTV
   using the supplied view dimensions.
4. `SkyPass::Record` writes the safe constant slot, binds pixel-stage b0, and
   draws a fullscreen triangle using three generated vertices.
5. Transition to present, close/execute, present, signal the slot's fence, and
   rotate the scheduler slot.

CPU and HLSL sky constants are five float4 values (80 bytes): camera right, up,
forward, view parameters (aspect and tan-half-FOV), and sun direction/intensity.
C++ size/offset assertions pin offsets 0, 16, 32, 48, and 64; upload slots retain
256-byte alignment. View/environment CPU data are separate from this GPU layout.

There is no unconditional GPU wait at frame end. Allocators and constant slots
are reused only after their own fence completes. `Renderer` flushes before its
members are destroyed, so pass resources and back buffers outlive submitted work.
The flush signals fresh work to cover failures between submission and the usual
frame signal. Shutdown failures are reported without throwing from the destructor.
If fence event registration/waiting fails, the scheduler polls to completion (or
device removal) before reporting the error to preserve resource lifetime.

Future passes can be explicit Renderer members with explicit record calls. Their
resources must follow the same frame-slot and shutdown lifetime rules; no generic
pass abstraction is needed for that extension.
