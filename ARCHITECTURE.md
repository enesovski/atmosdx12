# AtmosDX12 architecture

`Application` owns input, the Win32 `Window`, the camera, the renderer and the clock.
The window holds a non-owning reference to input, which outlives it. Each loop pumps
messages, applies the existing mouse sensitivities, and passes camera vectors, FOV
projection scale and elapsed seconds to `Renderer` in a small `FrameData` value.
Minimizing suspends rendering with `WaitMessage`; elapsed time continues to advance.

`Renderer` owns four concrete components, in dependency order:

- `D3D12Context`: DXGI factory, selected hardware adapter, device and direct queue.
- `SwapChain`: two RGBA8 flip-discard back buffers and their RTV heap; vsynced present.
- `FrameScheduler`: two command allocators, one reusable command list, a fence and
  an RAII event handle. Its queue pointer borrows the context's longer-lived queue.
- `SkyPass`: the existing fullscreen pipeline and a mapped `UploadBuffer` containing
  two 256-byte-aligned constant slots. `GraphicsPipeline` owns the root signature
  and PSO and keeps shader compilation private to its implementation.

The frame lifecycle is explicit in `Renderer::Render`:

1. `BeginFrame` waits only for the selected scheduler slot's previous fence before
   resetting its allocator and the command list. This also makes its upload slot safe.
2. Fetch the current back buffer from DXGI independently of the scheduler slot.
3. Transition present → render target; clear and set viewport, scissor and RTV.
4. `SkyPass::Record` writes that slot's constants, binds b0, and draws three vertices.
5. Transition render target → present, close/execute, present, signal the slot's
   fence, and rotate to the next scheduler slot.

There is no unconditional GPU wait at the end of a frame. Allocators and constant
slots are reused only after their own fence completes, even with a slow GPU.
The renderer flushes before its members are destroyed, so pass resources and back
buffers outlive submitted work. The flush signals the queue again to cover failures
between submission and the normal end-of-frame signal. Shutdown wait failures are
reported without throwing from the destructor.
If registering or waiting on the fence event fails, the scheduler polls the fence
to completion (or device removal) before reporting the error, preserving resource
lifetime even when event registration runs out of memory.

`Camera` remains independent of D3D12. Passes consume `FrameData` rather than camera
or clock objects. The HLSL, four-float4 constant layout, shader bindings, pipeline
state, fixed window style and input calculations are preserved. A future pass can
be another concrete renderer member recorded alongside `SkyPass`; no pass interface
or resource registry is needed yet.
