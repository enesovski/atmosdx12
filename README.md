# AtmosDX12

A real-time atmospheric rendering project built with **C++20, Direct3D 12, and HLSL**.

> **Status:** Early development. Currently initializes a Direct3D 12 hardware device. Rendering features are planned, not yet implemented.

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

The current executable prints the selected GPU and confirms Direct3D 12 initialization. It does not open a rendering window yet.

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
