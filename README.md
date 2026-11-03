# AtmosDX12

A real-time atmospheric rendering project built with **C++20, Direct3D 12, and HLSL**.

> **Status:** Early development. Renders a procedural sky with a moving sun, a day/night cycle, and mouse camera controls.

## Planned Features

- Procedural sky and day/night cycle
- Raymarched volumetric clouds
- Height fog, volumetric scattering, and sunlight shafts
- Weather-driven lighting and smooth transitions
- Half-resolution rendering and temporal reconstruction
- Optional real-world weather and desktop wallpaper integration

## Requirements

- Windows 10/11 and a Direct3D 12-capable GPU
- Visual Studio C++ Build Tools (MSVC) and Windows SDK
- CMake 3.21 or newer

## Build

Open the project folder in VS Code with **C/C++** and **CMake Tools**, then select an x64 MSVC kit and run **CMake: Configure** and **CMake: Build**.

Alternatively, use a **Developer PowerShell for Visual Studio** shell:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

The executable prints the selected GPU and opens a fixed 1280×720 rendering window.
Hold the right mouse button to look around and use the mouse wheel to adjust FOV.
The sky's day/night cycle runs continuously, including while the window is minimized.

Shaders are copied beside the executable and resolved relative to it, so the program
can be launched from any working directory. See [ARCHITECTURE.md](ARCHITECTURE.md)
for resource ownership and the frame lifecycle.

## Roadmap

1. Direct3D 12 initialization and first clear frame
2. Swapchain, render targets, and fullscreen HLSL sky shader
3. Volumetric clouds and atmospheric scattering
4. Fog and sun shafts
5. Temporal reconstruction and GPU profiling
6. Weather simulation and optional live weather integration

## References

- [Direct3D 12 documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics)
- [Microsoft D3D12 samples](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld)
