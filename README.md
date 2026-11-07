# AtmosDX12

A lightweight atmospheric renderer built with **C++20, Direct3D 12, and HLSL**.

The current renderer draws a stylized sky gradient with a sun disk, glow, and
horizon/twilight colors driven by sun elevation. It is a specialized renderer,
with no time simulation or control UI.

## Requirements

- Windows 10/11 and a Direct3D 12-capable hardware GPU
- Visual Studio C++ Build Tools (MSVC) and Windows SDK
- CMake 3.21 or newer

## Build and run

From a **Developer PowerShell for Visual Studio** shell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
.\build\Debug\AtmosDX12.exe
```

The executable prints the selected GPU and opens a fixed 1280x720 window.
Hold the right mouse button to look around and use the mouse wheel to adjust FOV.
Minimizing suspends rendering until another window message arrives.

Set `sunAzimuthDegrees`, `sunElevationDegrees`, and `sunIntensity` in
`src/scene/SkySettings.h` to change the default sky. +Y is up; azimuth zero points
along +Z, increasing toward +X. Elevation is measured above the horizon.
The defaults approximate the former initial sun position.

Shaders are copied beside the executable and resolved relative to it, so the
program can be launched from any working directory. They compile at startup
with Shader Model 5.1; Debug enables shader debug information and disables
optimization. CMake builds C++ and copies HLSL; it does not compile HLSL.

The renderer consumes camera-only `RenderView` data and separate `SkySettings`.
Passes live in `src/graphics/passes`; reusable Direct3D 12 infrastructure lives
in `src/graphics/d3d12`. Rendering order is explicit and GPU frame-slot
synchronization is retained.

See [ARCHITECTURE.md](ARCHITECTURE.md) for ownership and frame synchronization.

## Future direction

Geometry rendering, HDR, volumetric fog, volumetric clouds, and post-processing
are future work. The current code contains only the infrastructure and sky pass
it uses today; it is not a general-purpose game engine.

## References

- [Direct3D 12 documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics)
- [Microsoft D3D12 samples](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld)
