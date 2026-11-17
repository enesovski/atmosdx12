# AtmosDX12

A real-time render engine in development, built with **C++20, Direct3D 12 and HLSL (SM 6.6)**.

Today it renders a procedural sky with a sun disk into an HDR target and tonemaps it to the
screen. Underneath is the engine foundation: a bindless resource model, a DXC shader pipeline,
per-frame upload memory, tracked resource states and debug tooling. Geometry, materials,
lighting and a physically based atmosphere come next. See [ROADMAP.md](ROADMAP.md).

## Requirements

- Windows 10/11 and a GPU and driver with **Shader Model 6.6** and **resource binding tier 3**
  (NVIDIA Turing+, AMD RDNA+, Intel Xe+)
- Visual Studio 2022+ C++ tools and Windows SDK 10.0.22621 or newer (DXC ships with it)
- CMake 3.24 or newer
- Optional: the Windows "Graphics Tools" feature, for the D3D12 debug layer

## Build and run

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
.\build\app\Debug\AtmosDX12.exe
```

- Hold the right mouse button to look around; use the mouse wheel to change the FOV.
- The window can be resized; minimizing pauses rendering.
- `--gpu-validation` enables D3D12 GPU-based validation (slow).
- Debug builds enable the D3D12 debug layer and DRED. Debug-layer messages are printed to
  the console.

Shaders live in `engine/shaders`. Each build copies them next to the executable, and they
compile with DXC at startup.

## Layout

```text
engine/     AtmosEngine static library (core, platform, scene, graphics, shaders)
app/        AtmosDX12 executable: window, camera input, and the scene it renders
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for how the engine is structured.

## References

- [Direct3D 12 documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics)
- [HLSL Shader Model 6.6 dynamic resources](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html)
