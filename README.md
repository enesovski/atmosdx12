# AtmosDX12

A real-time render engine in development, built with **C++20, Direct3D 12 and HLSL (SM 6.6)**.

It loads scenes of glTF models asynchronously, including:

- textures cooked to block-compressed DDS
- normal maps
- alpha-tested and double-sided materials

It runs inside a docked editor in the style of Unity and Unreal (see below). Each frame runs through a render
graph:

- sun cascaded shadows and a local-light shadow atlas
- depth prepass and GBuffer
- clustered light culling and deferred GGX lighting in compute
- image-based lighting captured from the sky
- a sorted forward pass for transparents
- HDR tonemapping, then editor overlays (wireframe, selection outline)

Underneath are:

- a bindless resource model with vertex pulling
- a DXC shader pipeline
- copy-queue streaming
- tracked resource states and deferred resource release

Next come temporal anti-aliasing and HDR post-processing, screen-space and ray-traced reflections and AO, global
illumination, and a physically based atmosphere. See [ROADMAP.md](ROADMAP.md).

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
| `--scene file` | Open a `.json` scene, or a single `.gltf`/`.glb` model (default: the last scene, then `scenes/sponza.json`) |
| `--dump-frame` | Print the first frame's render graph and culling stats |
| `--gpu-validation` | D3D12 GPU-based validation (slow) |
| `--spawn-lights N` | Add N random point lights (stress test for the light grid) |

The app is an editor in the style of Unity and Unreal. Its panels dock, tab and resize, and the layout is saved
between runs:

- **Viewport:** view modes, show flags, camera menu with bookmarks, stats.
- **Hierarchy** and **Inspector:** entities, transforms, lights, models.
- **Render Settings:** quality presets, shadows, sky lighting.
- **Content Browser**, **Console**, **Profiler** (per-pass GPU timings) and **Render Graph**.

**Window > Layout** switches between the Default, Lighting and Profiling layouts.

| Input (viewport) | Action |
|---|---|
| Hold right mouse button | Fly: mouse look, WASD move, Q/E down/up, Shift faster, wheel changes speed |
| Alt + left drag | Orbit around the focused point |
| Middle drag | Pan |
| Mouse wheel | Dolly toward the focused point |
| Click | Select (Ctrl toggles, Shift adds) |
| F | Focus the selection |
| Q / W / E / R | Select / move / rotate / scale tool |
| Ctrl+1..9 / 1..9 | Save / recall a camera bookmark |
| Alt+1..9 | View modes (Lit, Lighting Only, buffers, cascades, light complexity) |

| Input (anywhere) | Action |
|---|---|
| Ctrl+Z / Ctrl+Y | Undo / redo |
| Ctrl+D / Delete / F2 | Duplicate / delete / rename |
| Ctrl+N / Ctrl+O / Ctrl+S | New / open / save scene (F5 also saves; the current view becomes the start camera) |
| F11 or F1 | Fullscreen viewport |

From Blender, export with **File → Export → glTF 2.0**, then add the file to a scene's `entities` or open it with
`--scene`.

## Scene files

```json
{
  "version": 2,
  "environment": { "sunAzimuthDegrees": 115, "sunElevationDegrees": 60, "sunIntensity": 8, "exposure": 1 },
  "camera": { "position": [-7.5, 2.2, -0.3], "yawDegrees": 90, "pitchDegrees": 8, "fovDegrees": 65 },
  "entities": [
    { "name": "Sponza", "parent": -1, "position": [0, 0, 0], "rotationDegrees": [0, 0, 0], "scale": [1, 1, 1],
      "model": "../assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf" },
    { "name": "Fire", "parent": -1, "position": [-9.5, 1.2, 3.8], "rotationDegrees": [0, 0, 0], "scale": [1, 1, 1],
      "light": { "type": "point", "color": [1, 0.55, 0.25], "intensity": 12, "range": 8, "castsShadows": true } }
  ]
}
```

- **Model paths** are relative to the scene file.
- **`parent`** is an index into `entities`, or -1 for none. A parent must come before its children.
- **`active: false`** hides an entity and its children.
- **`renderSettings`** and **`cameraBookmarks`** are written by the editor; missing keys keep their defaults, and
  version 1 files still load.
- **`light`** makes an entity a `"point"` or `"spot"` light: `color`, `intensity`, `range` (meters), `castsShadows`,
  and for spots `innerConeDegrees`/`outerConeDegrees`. A spot shines along the entity's +Z (rotate it with
  `rotationDegrees`). `sunIntensity` is the sun's illuminance on the same scale.

## Layout

```text
engine/     AtmosEngine static library (core, platform, scene, assets, runtime, graphics, shaders); no UI code
editor/     AtmosEditor library: editor shell, panels, theme, tools (ImGui docking + ImGuizmo)
app/        AtmosDX12 executable: command line, runs the editor
Saved/      Per-user editor state: dock layout and preferences (git-ignored)
scenes/     Scene files
docs/       Per-phase plans and design decisions
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for how the engine is structured.

## References

- [Direct3D 12 documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics)
- [HLSL Shader Model 6.6 dynamic resources](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html)
- [glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
