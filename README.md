# AtmosDX12

A real-time render engine in development, built with **C++20, Direct3D 12 and HLSL (SM 6.6)**.

It loads scenes of glTF models asynchronously, including:

- textures cooked to block-compressed DDS
- normal maps
- alpha-tested and double-sided materials

Each frame runs through a render graph: depth prepass, forward opaque pass, procedural sky and HDR tonemapping, with
an ImGui debug overlay on top. Underneath are:

- a bindless resource model with vertex pulling
- a DXC shader pipeline
- copy-queue streaming
- tracked resource states and deferred resource release

PBR lighting, shadows and a physically based atmosphere come next. See [ROADMAP.md](ROADMAP.md).

## Requirements

- Windows 10/11 and a GPU and driver with **Shader Model 6.6** and **resource binding tier 3**
  (NVIDIA Turing+, AMD RDNA+, Intel Xe+)
- Visual Studio 2022+ C++ tools and Windows SDK 10.0.22621 or newer (DXC ships with it)
- CMake 3.25 or newer. The first configure downloads cgltf, nlohmann/json, DirectXTex and Dear ImGui from GitHub.
- Optional: the Windows "Graphics Tools" feature, for the D3D12 debug layer

## Get the Sponza scene

The default scene, `scenes/sponza.json`, expects the Khronos sample assets in `assets/` (git-ignored):

```powershell
git clone --depth 1 --filter=blob:none --sparse https://github.com/KhronosGroup/glTF-Sample-Assets.git assets/glTF-Sample-Assets
```
```powershell
git -C assets/glTF-Sample-Assets sparse-checkout set Models/Sponza
```

## Build and run

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
.\build\app\Debug\AtmosDX12.exe
```

The first launch cooks Sponza's 69 textures in the background (a few seconds). The scene appears right away, and
textures pop in as they finish. Later launches load the cooked files from `build\app\<config>\AssetCache`.

| Option | Effect |
|---|---|
| `--scene file` | Load a `.json` scene, or a single `.gltf`/`.glb` model |
| `--dump-frame` | Print the first frame's render graph and culling stats |
| `--gpu-validation` | D3D12 GPU-based validation (slow) |

| Input | Action |
|---|---|
| Hold right mouse button | Fly: mouse look, cursor hidden |
| RMB + W / A / S / D | Move |
| RMB + Q / E | Down / up |
| RMB + Shift | Move faster |
| RMB + mouse wheel | Change fly speed |
| Mouse wheel | Field of view |
| F1 | Show/hide the debug UI (stats, render graph, asset progress, sun and exposure sliders) |
| F5 | Save the scene, including the current camera as its start view |

From Blender, export with **File → Export → glTF 2.0**, then add the file to a scene's `entities` or open it with
`--scene`.

## Scene files

```json
{
  "version": 1,
  "environment": { "sunAzimuthDegrees": 115, "sunElevationDegrees": 60, "sunIntensity": 2.5, "exposure": 1 },
  "camera": { "position": [-7.5, 2.2, -0.3], "yawDegrees": 90, "pitchDegrees": 8, "fovDegrees": 65 },
  "entities": [
    { "name": "Sponza", "parent": -1, "position": [0, 0, 0], "rotationDegrees": [0, 0, 0], "scale": [1, 1, 1],
      "model": "../assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf" }
  ]
}
```

- **Model paths** are relative to the scene file.
- **`parent`** is an index into `entities`, or -1 for none. A parent must come before its children.

## Layout

```text
engine/     AtmosEngine static library (core, platform, scene, assets, runtime, graphics, ui, shaders)
app/        AtmosDX12 executable: window loop, fly camera, debug panels
scenes/     Scene files
docs/       Per-phase plans and design decisions
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for how the engine is structured.

## References

- [Direct3D 12 documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics)
- [HLSL Shader Model 6.6 dynamic resources](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html)
- [glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
