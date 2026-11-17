# AtmosDX12

A real-time render engine in development, built with **C++20, Direct3D 12 and HLSL (SM 6.6)**.

It renders 3D meshes loaded from glTF with a depth prepass, a forward sun-lit opaque pass, a procedural sky and HDR
tonemapping. Every frame is scheduled by a render graph. Underneath are:

- a bindless resource model with vertex pulling
- a DXC shader pipeline
- per-frame upload memory
- tracked resource states and deferred resource release
- debug tooling

PBR materials, real lighting and shadows, and a physically based atmosphere come next. See [ROADMAP.md](ROADMAP.md).

## Requirements

- Windows 10/11 and a GPU and driver with **Shader Model 6.6** and **resource binding tier 3**
  (NVIDIA Turing+, AMD RDNA+, Intel Xe+)
- Visual Studio 2022+ C++ tools and Windows SDK 10.0.22621 or newer (DXC ships with it)
- CMake 3.24 or newer; the first configure downloads cgltf from GitHub
- Optional: the Windows "Graphics Tools" feature, for the D3D12 debug layer

## Build and run

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
.\build\app\Debug\AtmosDX12.exe
```

Without arguments it shows a built-in test scene: a ground plane and a few cubes.

| Option | Effect |
|---|---|
| `--scene path\to\model.gltf` | Load a glTF 2.0 file (`.gltf` or `.glb`) |
| `--dump-frame` | Print the first frame's render graph and culling stats |
| `--gpu-validation` | D3D12 GPU-based validation (slow) |

| Input | Action |
|---|---|
| Right mouse button + drag | Look around |
| W / A / S / D | Move |
| Q / E | Down / up |
| Shift | Move faster |
| Mouse wheel | Field of view |

The window can be resized, and minimizing pauses rendering. Debug builds enable the D3D12 debug layer and DRED, and
print debug-layer messages to the console.

### Test scenes

To try a real scene, download **Sponza** from
[Khronos glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Sponza) into
`assets/`, which git ignores, and run:

```powershell
.\build\app\Debug\AtmosDX12.exe --scene assets\Sponza\glTF\Sponza.gltf
```

Only geometry and each material's base color are used for now. From Blender, use **File → Export → glTF 2.0**.

## Layout

```text
engine/     AtmosEngine static library (core, platform, scene, assets, graphics, shaders)
app/        AtmosDX12 executable: window, fly camera, and the scene it renders
docs/       Phase plans
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for how the engine is structured.

## References

- [Direct3D 12 documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics)
- [HLSL Shader Model 6.6 dynamic resources](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html)
- [glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
