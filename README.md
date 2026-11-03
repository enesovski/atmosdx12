# AtmosDX12

A real-time atmospheric rendering project built with DirectX 12.

The goal is to explore real-time graphics programming and create a stylized, interactive atmosphere with dynamic skies, clouds, lighting, and weather effects.

## Tech Stack

- **C++20**
- **DirectX 12**
- **HLSL**
- **Win32 API**
- **CMake**

## Features

- Modular DirectX 12 rendering architecture
- Procedural sky rendering
- Dynamic day/night cycle
- Sun and moon simulation
- Procedural stars
- Interactive camera controls
- Time-of-day and sky settings UI
- Frame-based GPU synchronization

## Getting Started

**Requirements:** Windows 10/11, a DirectX 12-compatible GPU, CMake, Visual Studio C++ Build Tools, and Windows SDK.

Clone the repository:

```bash
git clone https://github.com/enesovski/atmosdx12.git
cd atmosdx12
```

Configure and build using a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Run:

```powershell
.\build\Debug\AtmosDX12.exe
```

## Controls

- **Right Mouse Button + Drag:** Look around
- **Mouse Wheel:** Adjust field of view
- **Sky Controls:** Change time of day, day duration, and celestial intensities

## Planned Features

- Atmospheric scattering
- Volumetric clouds
- Volumetric fog and sun shafts
- Dynamic weather effects
- GPU optimizations and temporal rendering
- Unity native plugin
