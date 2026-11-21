# Phase 4 plan: editor UI

**Status:** ✅ implemented.

**Verification** (RTX 4060 Laptop, 1920x1080 at 125% scaling):

- **Builds:** `/W4` Debug and Release have no warnings; a GPU-validation run logged no `D3D12:` messages.
- **Checked on screen** by driving the UI with window messages:
  - default layout; hierarchy selection and the inspector (transform, light, model, Sky & Sun compass)
  - viewport picking of light icons; move gizmo; light range and cone overlays; selection outline
  - Window, Render and View Mode menus
  - Render Settings, Profiler, Console, Content Browser and Render Graph panels
- **GPU cost:** Release, Sponza selected (mask + outline passes running): 1.2 ms GPU for the whole frame at
  1146x616.
- **Not verified:**
  - other DPI scales or a second monitor
  - gizmo dragging, undo/redo, drag and drop and file dialogs (they need real mouse input)

**Deviations from this plan:**

- **Fonts:** Segoe UI, Cascadia Mono and Segoe Fluent Icons ship with Windows, so the editor loads them from the
  system instead of vendoring Inter, JetBrains Mono and an icon font. There are no binary files in the repository
  and nothing to download. Icon code points were checked by rendering them.
- **Undo** stores whole-scene snapshots instead of per-component snapshots. Every edit is covered the same way.
- **Visibility:** the hierarchy eye is the entity's saved `active` flag (Unity's active checkbox), not an
  editor-only hide state.
- **Multi-selection:** the inspector and gizmo edit only the primary entity.
- **Wireframe** is a show flag drawn over the lit image, not a separate view mode.
- **Light range and cone overlays** are drawn by ImGui, without depth test, instead of an engine line pass.
- **Not built yet:**
  - viewport grid and cascade-bounds show flags
  - texture thumbnails and cook-state badges in the content browser
  - shadow depth-bias settings (bias is baked into the shadow PSOs)
  - cluster far-depth setting
  - exposure in EV100 (with auto exposure in Phase 5)
- **ImGuizmo** is pinned to commit `22a6c90`, the last one that builds against ImGui 1.91.9.
- **First layout:** the default layout is built on the first frame after the window is maximized, because dock
  node sizes are absolute.

**Goal:** replace the single debug window with an editor shell that feels like Unity's or Unreal's. That means:

- dockable, resizable panels, a viewport panel, a hierarchy and an inspector
- a render-settings panel and viewport view modes
- one consistent theme with proper fonts and icons

Every later phase (TAA, SSR, ray tracing, GI) plugs its settings and view modes into this shell, so it comes first.

**Done when:**

- **Docking:** every panel docks, undocks, tabs and resizes; the layout survives restarts; *Window → Reset Layout*
  restores the default.
- **Viewport:** the scene renders into a viewport panel at the panel's size, and the camera only takes input while
  the viewport is hovered or focused.
- **Editing:**
  - Lights can be created, selected (in the hierarchy or by clicking their icon), moved with a gizmo, edited in the
    inspector and deleted.
  - Every edit undoes and redoes.
- **Settings and view modes:**
  - Every Phase 3 render setting is in the Render Settings panel, saved with the scene, with per-setting reset.
  - Every debug view is a viewport view mode.
- **Profiler** shows per-pass GPU timings.
- **Quality:**
  - Text is crisp at 100%, 150% and 200% Windows scaling.
  - `/W4` builds have no warnings, GPU validation is clean, and the editor costs under 0.5 ms of GPU time.

---

## Default layout

```text
+------------------------------------------------------------------------------------------------+
| File  Edit  Create  Window  Render  Help                                       AtmosDX12 - Sponza* |  menu bar
| [Select][Move][Rotate][Scale]  [Local|World]  [Snap 0.25]                       [Lit v] [Stats] |  toolbar
+--------------------+---------------------------------------------------+-----------------------+
| Hierarchy          | Viewport                                          | Inspector | Render    |
| [search........ ]  | [Lit v] [Perspective v] [Show v] [5.0 m/s]  2.8ms |   (tab)   | Settings  |
|  Sky & Sun         |                                                   |                       |
|  Sponza            |                                                   |  Fire West            |
|  Fire West   <sel> |                 scene image                       |  > Transform          |
|  Fire East         |                                                   |  > Light              |
|  Gallery Blue      |               (gizmo, light icons)                |  [Add Component]      |
|  Atrium Spot       |                                                   |                       |
+--------------------+---------------------------------------------------+-----------------------+
| Content Browser | Console | Profiler | Render Graph                                             |
|  scenes/  assets/  ...                                                                          |
+------------------------------------------------------------------------------------------------+
| Ready   |   Loading textures 42/69  [=====     ]   |   RTX 4060 Laptop   |   144 FPS   2.8 ms GPU |  status bar
+------------------------------------------------------------------------------------------------+
```

- **Proportions:** left column 18% (hierarchy); right column 22% (inspector, with Render Settings as a tab); bottom
  row 25% (content browser, console, profiler, render graph as tabs); the viewport takes the rest.
- **Built in code:** the default is built with `DockBuilder` and is the layout *Reset Layout* restores.
- **Layout presets** (Window → Layout):

  | Preset | Purpose | Differences from Default |
  |---|---|---|
  | Default | Editing the scene | as above |
  | Lighting | Tuning render settings | Render Settings docked full height on the right; Profiler docked under the viewport |
  | Profiling | Performance work | Profiler and Render Graph side by side under a smaller viewport |

- **Fullscreen viewport** (F11): hides every panel and shows only the scene, like Unreal's *Immersive Mode*. F11
  again restores the layout. F1 keeps its meaning: hide/show the UI.

---

## Panels

Each panel is one class in `editor/panels/`. It draws from an `EditorContext` (scene, selection, settings, undo
stack, renderer stats) and never owns scene data.

### Menu bar

| Menu | Items |
|---|---|
| File | New Scene, Open Scene..., Open Recent >, Save (Ctrl+S, also F5), Save As..., Exit |
| Edit | Undo (Ctrl+Z), Redo (Ctrl+Y), Duplicate (Ctrl+D), Delete (Del), Rename (F2), Focus Selection (F), Preferences... |
| Create | Empty Entity, Point Light, Spot Light, Model from file... |
| Window | Show/hide each panel, Layout > (Default, Lighting, Profiling), Reset Layout, Fullscreen Viewport (F11) |
| Render | View Mode >, Quality Preset >, Recapture Sky Light, Dump Frame Graph (to the console), Screenshot (F12) |
| Help | Controls (shortcut sheet), About |

- **Title:** the window title shows the scene name, with `*` when it has unsaved changes.
- **Unsaved changes:** closing, New or Open with unsaved changes asks *Save / Don't Save / Cancel*.

### Toolbar

- **Transform tool** (Q/W/E/R as in Unity and Unreal): Select, Move, Rotate, Scale.
- **Gizmo space:** Local/World toggle.
- **Snapping:** a toggle, plus the step size (move 0.25 m, rotate 15°, scale 0.1), each editable from a dropdown.
- **Right side:** the active view mode and a Stats overlay toggle, mirrored from the viewport.

### Viewport

The scene renders into an offscreen texture at the panel's exact pixel size, shown as an image filling the panel.

- **Viewport toolbar** (top-left overlay, Unreal-style):
  - **View mode** dropdown (table below).
  - **Camera** menu: field of view, move speed, sensitivity, near plane; *Camera bookmarks*: Ctrl+1..9 saves the
    current view and 1..9 jumps to it. Bookmarks are saved with the scene, so A/B comparisons in later phases always
    use the same shots.
  - **Show** flags: light icons, light ranges (wire spheres), spot cones, selection outline, grid, stats overlay,
    shadow cascade bounds.
  - **Resolution scale** (50–100%), which is the hook for upscaling in Phase 5.
- **Stats overlay** (top right, toggled): FPS, CPU ms, GPU ms, draws, lights, resolution.
- **Input:**
  - Hold RMB to fly (unchanged); the wheel zooms when not flying.
  - LMB selects; Shift+LMB adds to the selection.
  - F frames the selection; Alt+LMB orbits around it (Unreal/Unity style).
  - Keys reach the camera only while the viewport is hovered or focused.
- **Drag and drop:** a `.gltf`/`.glb` from the content browser creates an entity at the point under the cursor.

**View modes** (the existing `DebugView`, renamed and grouped like Unreal's view-mode menu):

| Group | Mode | Available |
|---|---|---|
| Lit | Lit | Phase 3 |
| Lit | Lighting Only (white albedo, normal maps kept) | **new, Phase 4** |
| Lit | Unlit (base color + emissive) | Phase 3 (Base color) |
| Wireframe | Wireframe overlay on Lit | **new, Phase 4** |
| Buffer | Base Color, World Normal, Roughness, Metallic, Ambient Occlusion | Phase 3 |
| Lighting | Shadow Cascades, Light Complexity | Phase 3 |
| Lighting | AO only, Reflections only, Direct only, Indirect only | Phase 6+ |
| Ray tracing | Path Tracer (reference) | Phase 7 |
| GI | Probe visualization, Irradiance only | Phase 8 |
| Post | Motion Vectors, Exposure (luminance heat map) | Phase 5 |

**Rule:** the UI only lists modes that exist. A later phase adds its entries to the table in code (one registration
per mode), so there are never dead menu items.

### Hierarchy (Unity: Hierarchy, Unreal: Outliner)

- **Tree:** entities in a tree that follows `parent`, with an icon per type (model, point light, spot light,
  empty).
- **Sky & Sun:** a pseudo-entity pinned at the top. Selecting it shows the environment in the inspector, as
  Unreal's *DirectionalLight* and *SkyLight* actors do.
- **Search:** filters by name and keeps matching children's parents visible.
- **Per row:**
  - an eye toggle (hide in viewport; editor-only, not saved)
  - a light-shadow indicator
- **Mouse:**
  - click selects; Ctrl+click toggles; Shift+click selects a range
  - double-click frames the entity in the viewport
- **Edit commands:** F2 renames; drag and drop reparents (drop on empty space makes the entity a root); Del deletes;
  Ctrl+D duplicates.
- **Context menu:** Create child >, Rename, Duplicate, Delete, Focus, Copy/Paste component values.

### Inspector (Unity: Inspector, Unreal: Details)

The inspector is a header plus one collapsible section per component. Every section is a **property grid**: a
resizable two-column table with the label (40%) and the value (fill).

- **Header:** an icon, an editable name, and an enabled checkbox (Phase 4 uses it for lights: an off light is not
  rendered).
- **Transform:**
  - Position, Rotation (degrees) and Scale as three-field rows. Each field has a red/green/blue X/Y/Z label and is
    dragged to edit or double-clicked to type.
  - A uniform-scale lock.
  - A reset arrow per row.
- **Light:**

  | Property | Widget | Range / unit |
  |---|---|---|
  | Type | dropdown | Point, Spot |
  | Color | color button + picker; optional *Use temperature* | RGB, or 1,000–20,000 K |
  | Intensity | drag float with unit suffix | 0–10,000 cd |
  | Range | drag float | 0.1–100 m |
  | Inner / Outer cone | slider pair; inner is clamped to outer | 0–80° (spot only) |
  | Cast Shadows | checkbox | |
  | Shadow bias override | drag floats, hidden behind an *Advanced* toggle | **new** |

- **Model:**
  - path (read-only, with a *Browse* button), load state, submesh, material and triangle counts
  - a read-only material list: base color swatch, texture slots as small thumbnails, alpha mode, double-sided
- **Sky & Sun** (pseudo-entity):
  - sun azimuth and elevation, plus a small compass widget that drags the sun direction
  - sun intensity (lux), exposure (EV)
- **Add Component:** a searchable popup (Light, Model). *Remove Component* is in each section's context menu.
- **Modified values** (different from the component's default) show a reset arrow, like Unreal's yellow arrow.
- **Multi-selection** edits shared components together; a field whose values differ shows `—`.

### Render Settings (Unity: Lighting window / HDRP Volume, Unreal: Post Process Volume)

- **Quality preset** at the top: Low / Medium / High / Ultra / Custom. Editing any value switches to Custom.
- **Categories** below are collapsible sections. Each has an enable checkbox in the header where the feature can be
  switched off, and a *Reset category* item in its context menu.
- **Saved** with the scene as a `"renderSettings"` block. The view mode is not saved.

| Category | Settings (today) | Range | Engine work |
|---|---|---|---|
| Exposure | Exposure (EV100) | -6 to 16 | replaces the linear multiplier |
| Sun Shadows | Enabled, Distance, Cascade split (λ), Normal offset (texels), Depth bias, Slope bias, Cascade resolution (1024/2048/4096), Filter (Hard / PCF 3x3) | 10–500 m, 0–1, 0–5 | bias, resolution and filter become settings |
| Local Shadows | Enabled, Atlas resolution (2048/4096/8192), Tile size (256/512/1024), Max shadowed lights | | atlas sizes become settings |
| Sky Lighting | Enabled (IBL), Ambient intensity, Specular intensity, *Recapture now* button | 0–3 | separate diffuse/specular scale |
| Lights | Max lights per cluster (readout), Cluster depth range | 50–2000 m | far depth becomes a setting |
| Debug | Freeze culling, Show cascade bounds | | new show flags |

- **Later phases** each add a category here, like *Anti-aliasing* (TAA), *Ambient Occlusion*, *Reflections*,
  *Ray Tracing*, *Global Illumination*, *Bloom*, *Tonemapping* and *Fog*. The panel is generic over a settings
  description, so a new category is one registration.

### Profiler

- **Frame graph:** CPU and GPU frame time over the last 300 frames, with a 16.6 ms / 8.3 ms guide line. Pause
  freezes the history.
- **GPU passes table:** pass name, GPU ms (averaged over 30 frames), % of the frame, a bar. Sortable; culled passes
  are grey.

  This needs per-pass timestamps: the render graph wraps each live pass in a timestamp pair, reusing the
  `GpuTimer` query heap, read back three frames later.
- **Counters:** draws (camera/shadow), lights, shadowed lights, clusters at the 128-light cap, transient textures
  and buffers, VRAM usage and budget (`IDXGIAdapter3::QueryVideoMemoryInfo`).

### Render Graph

- The live and culled pass list. Selecting a pass shows its reads and writes with states, clear/load ops and
  subresources.
- A resource list: name, size, format, transient/imported, first and last use.
- *Dump to console* button (same as `--dump-frame`).

### Console

- **Capture:** the engine log, through a new `log::AddSink` with a ring buffer of 2,000 lines.
- **Display:** level filters (Info/Warning/Error) with counts, text search, Clear, auto-scroll, copy line.
  `D3D12:` debug-layer messages are highlighted.
- **Errors:** any error makes the status bar's message area flash red until the console is opened.

### Content Browser

- **Layout:** a folder tree on the left (project `scenes/` and `assets/`) and a grid or list of files on the
  right. Icons by type: scene, glTF model, texture.
- **Actions:** double-click a scene to open it; drag a model into the viewport or hierarchy; right-click *Show in
  Explorer*.
- **Status:** a model's cook and load state (ready, loading, failed) shows as a corner badge.
- **Not yet:** thumbnails; asset icons are enough for Phase 4.

### Preferences (floating window)

| Setting | Values |
|---|---|
| UI scale | Auto (monitor DPI), 75–200% |
| Font size | 13–18 px at 100% scale (default 15) |
| Theme | Dark (default), Darker, Light (later) |
| Camera | Look sensitivity, default move speed, invert Y |
| Viewport | Selection outline color, grid spacing, show stats by default |
| Autosave | Off / every 5 minutes (writes `<scene>.autosave.json`) |

### Status bar

From left to right:

- the last log message, colored by level
- asset loading progress: count and a thin bar
- GPU name
- FPS and GPU ms

---

## Style plan

**Direction:** neutral dark greys like Unreal 5, with one blue accent like Unity. Flat panels without gradients,
1-pixel separators, and small rounding. Contrast comes from value steps between surfaces, not from borders.

### Fonts

| Role | Font | Size at 100% | Use |
|---|---|---|---|
| UI | **Inter** Regular | 15 px | All labels and values. Unity's editor uses Inter. |
| UI strong | Inter SemiBold | 15 px | Section headers, panel tabs, selected items |
| Small | Inter Regular | 13 px | Status bar, units, hints |
| Mono | **JetBrains Mono** Regular | 14 px | Console, render graph, numeric tables in the profiler |
| Icons | **Lucide** (or Font Awesome 6 Free Solid), merged into the UI and Mono fonts | 15 px | Toolbar, hierarchy, menus, buttons |

- **Atlas:** fonts are rasterized at the monitor's DPI scale (per-monitor DPI v2 awareness) and rebuilt on
  `WM_DPICHANGED`. Oversampling is 2×1 with pixel snapping off for Inter. FreeType rendering (`imgui_freetype`) is
  a later option if hinting looks soft.
- **Licenses:** all three are under permissive licenses (SIL OFL / ISC). The TTF files are checked in under
  `editor/resources/fonts/`, with their license files. This is a deliberate exception to the FetchContent rule:
  fonts are binary resources, not code, about 1 MB in total.

### Color tokens

Defined once in `EditorTheme` and mapped onto `ImGuiStyle::Colors`. Panels never use raw colors.

| Token | Dark | Used for |
|---|---|---|
| `Background` | `#151515` | Dock space, behind panels |
| `Panel` | `#1F1F1F` | Window background |
| `PanelRaised` | `#262626` | Headers, toolbars, menu bar, active tab |
| `Field` | `#121212` | Input fields, sliders, combo boxes |
| `FieldHover` | `#2B2B2B` | Hovered fields and buttons |
| `Border` | `#0B0B0B` | Panel separators, field outlines |
| `Text` | `#D9D9D9` | Default text |
| `TextDim` | `#8C8C8C` | Labels in the property grid, disabled text, units |
| `Accent` | `#3B8EEA` | Selection, active tab top line, checkbox marks, slider grabs, focus |
| `AccentDim` | `#24476B` | Selected row background, text selection |
| `Warning` | `#E5A50A` | Warnings, modified-value arrows |
| `Error` | `#E5484D` | Errors, failed assets |
| `Success` | `#46A758` | Ready assets |
| `AxisX` / `AxisY` / `AxisZ` | `#E5484D` / `#6BBF59` / `#4C8DF6` | Transform field labels, gizmo |

### Metrics

| Property | Value at 100% | Notes |
|---|---|---|
| Window padding | 8, 8 | 4, 4 in the viewport (image fills the panel) |
| Frame padding | 6, 4 | Field height = 15 px font + 8 = 23 px |
| Item spacing | 6, 4 | Inner spacing 4, 4 |
| Indent | 14 | Hierarchy tree indent |
| Rounding | frames 3, windows 4, tabs 3, popups 4, grabs 2 | |
| Borders | windows 1, frames 0, popups 1, tabs 0 | |
| Scrollbar | 12 px, rounding 6 | |
| Tab bar | Active tab: `PanelRaised` with a 2 px `Accent` top line; inactive: `Panel`; unfocused-active: no accent | Unreal-style tabs |
| Title bars | Hidden for docked panels (the tab acts as the title) | ImGui docking `NoWindowMenuButton` |

Everything is multiplied by the UI scale (`ImGuiStyle::ScaleAllSizes`).

### Reusable widgets (`editor/widgets/`)

- **`PropertyGrid`:** `Begin/End`, plus `Float`, `Float3` (axis-colored), `Color`, `Enum`, `Bool`, `Angle`,
  `Path`. Each draws the label column, the value, the unit suffix and the reset arrow. It returns *changed* and
  *committed* (on release), so undo can merge a whole drag into one step.
- **`SectionHeader`:** a full-width `PanelRaised` header with an icon, SemiBold text and an optional enable
  checkbox.
- **`IconButton` / `ToggleButton`:** square, icon-only buttons with a tooltip, for toolbars.
- **`SearchField`:** an input with a magnifier icon and a clear button.
- **`DropdownMenu`:** a toolbar dropdown that looks like a button (view mode, camera, show flags).

---

## Architecture

### Modules

```text
app/ (AtmosDX12.exe)        main, command line; creates the EditorApp
editor/ (AtmosEditor.lib)   new; the only code that includes ImGui
   EditorApp                frame loop: input, camera, panels, scene edits, Renderer::Render
   EditorContext            scene, selection, undo stack, render settings, prefs, renderer stats
   ImGuiLayer               ImGui Win32 + D3D12 backends (moved from engine/ui/DebugUi), an IOverlayRenderer
   style/                   EditorTheme (tokens -> ImGuiStyle), EditorFonts, Icons.h
   widgets/                 PropertyGrid, SectionHeader, IconButton, SearchField, DropdownMenu
   panels/                  Viewport, Hierarchy, Inspector, RenderSettings, Profiler, RenderGraph, Console,
                            ContentBrowser, Preferences, MainMenu, Toolbar, StatusBar
   tools/                   CameraController (fly + orbit), Gizmo (ImGuizmo), Picking, UndoStack
engine/                     no ImGui; engine/ui/ is removed
```

- **Layering:** *the renderer never includes ImGui* becomes *the engine never includes ImGui*.
- **App:** `app/` shrinks to `main.cpp`. `FlyCameraController` moves into `editor/tools/`.
- **CMake:** the editor library goes into the explicit source list like the engine.

### Engine changes

1. **Viewport output:**
   - `Renderer::Render` takes an optional `RenderTargetInfo { Texture* target; uint32_t width, height; }`.
     - With a target, `SceneRenderer` renders at the target's size and tonemaps into it. The overlay pass then
       declares `ReadTexture(target)` and draws the UI into the back buffer.
     - Without one (F11 fullscreen, or a future game build), it renders straight to the back buffer as today.
   - The viewport texture is persistent, owned by the viewport panel, and recreated when the panel size changes
     (deferred release makes that safe).
   - ImGui shows it through its bindless SRV: `ImTextureID` = the GPU descriptor handle.
2. **Per-pass GPU timestamps** in `RenderGraph::Execute`, exposed in `RenderPassInfo` as `gpuMilliseconds`.
3. **Log sinks:** `log::AddSink(callback)` so the console can capture messages.
4. **Render settings growth:**
   - exposure in EV100
   - shadow bias, resolution and filter
   - atlas sizes
   - separate diffuse and specular sky intensity
   - cluster far depth
   - `RenderSettings` serialization in `SceneSerializer`
5. **View modes:** *Lighting Only* (the deferred pass uses white albedo); *Wireframe* (an overlay pass that redraws
   the camera draw list with a wireframe PSO and depth test, alpha-blended).
6. **Editor overlays rendered by the engine:**
   - a selection outline: selected draws write a stencil mask, then a fullscreen edge pass. The draw list gets a
     per-draw entity id.
   - light range spheres and spot cones, drawn as a line list from a small CPU vertex buffer
7. **Light component:** an `enabled` flag; per-light shadow bias overrides.
8. **Scene file:** `"renderSettings"` and `"cameraBookmarks"` blocks; the format version goes to 2, and version 1
   files still load.

### Editor state and persistence

| What | Where | Format |
|---|---|---|
| Dock layout | `Saved/Editor/Layout.ini` (project root, git-ignored, like Unreal's `Saved/`) | ImGui ini |
| Preferences, recent scenes, last scene | `Saved/Editor/EditorPrefs.json` | JSON |
| Render settings, camera bookmarks | the scene file | JSON |
| Selection, hidden-in-viewport flags, panel visibility | not saved (visibility lives in the ini) | |

### Undo/redo

- **Snapshot commands:** each edit records `{ entity id, component before, component after }`. Create and delete
  record the whole entity and its children; reparent records the old and new parent.
- **Grouping:**
  - Drag edits merge until the widget reports *committed*, so one gizmo drag is one undo step.
  - A multi-selection edit is one compound command.
- **Limit:** 256 steps. Saving marks the current step as the clean point for the `*` indicator.

### Selection and picking

- **Selection:** `EditorSelection` holds entity ids, with the primary one last. It is cleared when its entities are
  deleted.
- **Picking (CPU, Phase 4):**
  - Lights are picked through their screen-space icons, nearest within 12 px.
  - Models are picked by ray against the world bounds of each submesh, nearest hit; that is enough for whole-entity
    selection.
  - A GPU id-buffer pick (Unity's and Unreal's hit proxies) is deferred until scenes have many small entities.

---

## Steps

Each step builds without warnings, runs with GPU validation clean, and keeps the app usable.

### S1. Shell, theme and fonts

- **Dependencies:**
  - Switch Dear ImGui to the docking tag (`v1.91.9b-docking`).
  - Add ImGuizmo (pinned commit) to `ThirdParty.cmake`.
  - Add the font files.
- **Module:** create `editor/`; move `DebugUi` → `ImGuiLayer` and the frame loop → `EditorApp`.
- **Style and docking:**
  - `EditorTheme`, `EditorFonts`, DPI awareness and atlas rebuild.
  - A dock space over the whole window, the main menu bar and the status bar.
  - The default layout through `DockBuilder`, ini persistence in `Saved/Editor/`, and Reset Layout.
- The old `DebugPanels` content moves into temporary panels, so nothing is lost while the rest is built.

### S2. Viewport

- **Rendering:** the `RenderTargetInfo` path in `Renderer`, the viewport texture and resize, and the overlay
  reading the texture.
- **Panel:** the viewport panel with its toolbar (view mode, camera menu, show flags, resolution scale) and the
  stats overlay.
- **Input:** camera input gated on viewport hover/focus; orbit (Alt+LMB) and focus (F); camera bookmarks.
- **Fullscreen:** F11.

### S3. Hierarchy and selection

- `EditorSelection` and the hierarchy panel with every interaction listed above.
- The Sky & Sun pseudo-entity.
- Create, duplicate, delete, rename and reparent, all through scene-edit functions.

### S4. Inspector and property widgets

- **Widgets:** `PropertyGrid` and the other widgets.
- **Sections:** Transform, Light (with color temperature), Model (read-only material list), Sky & Sun.
- **Components:** Add/Remove Component, multi-selection editing.
- **Engine:** the Light `enabled` flag.

### S5. Undo/redo

- `UndoStack` with snapshot commands, drag merging and the clean point.
- Every S3/S4 edit goes through it.
- Ctrl+Z / Ctrl+Y, and the `*` indicator.

### S6. Viewport tools

- **Gizmos:** ImGuizmo (move/rotate/scale, local/world, snapping) and the toolbar tool buttons with Q/W/E/R.
- **Lights:** icons drawn on the viewport, CPU picking, and range/cone wireframes.
- **Selection outline:** the engine pass (stencil mask + edge).

### S7. Render Settings panel and view modes

- **Panel:** a settings description table (category, label, pointer, range, unit, default), a generic panel,
  quality presets and per-setting reset.
- **Engine settings:** the new settings from *Engine changes* item 4, and `renderSettings` in the scene file
  (version 2).
- **View modes:** the view-mode registry; *Lighting Only* and *Wireframe*.

### S8. Profiler, Render Graph and Console

- **Profiler:** per-pass GPU timestamps in the render graph, the history graph, the pass table and VRAM counters.
- **Render Graph:** the panel with pass and resource details.
- **Console:** `log::AddSink`, the console panel, and the status-bar message.

### S9. Content Browser, files, Preferences

- **Files:** New / Open / Save As / Open Recent through Win32 file dialogs, and the unsaved-changes prompt.
- **Content Browser:** the panel, and drag and drop into the viewport and hierarchy.
- **Preferences:** the window, and autosave.

### S10. Verification and docs

- **Displays:** check at 100/150/200% scaling and with a second monitor at a different DPI.
- **Edit cycle:** create, edit, undo, save, reload Sponza's lights; confirm the layout survives restarts.
- **Validation and cost:** GPU validation; editor GPU cost (UI + outline + overlays) under 0.5 ms in the profiler.
- **Docs:**
  - ARCHITECTURE.md: editor module, viewport path, undo.
  - CLAUDE.md: layering (*the engine never includes ImGui*), new `editor/` sources, the font exception, view-mode
    and settings registration.
  - README: editor controls.
  - This plan: status.

---

## Decisions, alternatives, and who uses what

| Area | Chosen | Alternatives | Why |
|---|---|---|---|
| UI toolkit | Dear ImGui (docking branch), heavily themed | Qt (Maya, Houdini, O3DE); custom retained UI (Unreal Slate, Unity UI Toolkit, Godot) | Already integrated, immediate mode suits fast-changing renderer settings, and docking covers the editor shell. A retained toolkit is a project of its own. |
| Detached panels | Docking inside the main window only | ImGui multi-viewport (panels as OS windows) | Multi-viewport needs extra swap chains through the backend and complicates the frame loop; it can be enabled later without changing panels. |
| Viewport | Render to an offscreen texture shown in a panel | Render to the back buffer and leave a hole for the viewport | The texture is a clean, resizable panel (Unity, Unreal, Godot) and later supports several viewports; the cost is one extra texture read. |
| Editor location | A separate `editor/` library | Keep panels in `app/`; put the editor inside `engine/` | Matches the Unreal/Unity split between editor and runtime; the engine stays free of ImGui. |
| Inspector | Hand-written sections on a shared `PropertyGrid` | A reflection system (Unreal `UPROPERTY`, Unity serialized fields, O3DE RTTI) | Only three components exist; reflection pays off once there are dozens. The grid keeps every section uniform until then. |
| Render settings UI | A settings description table drawn by one generic panel | Hand-written widgets per setting; console variables (Unreal `r.*`) | Each later phase adds a category in one place, with ranges, defaults and reset for free. Console variables can be layered on the same table later. |
| Undo | Component snapshots | Command objects per operation; transaction log (Unreal `FTransaction`) | Components are small value types, so snapshots are trivially correct and need no per-operation undo code. |
| Picking | CPU: light icons + ray vs submesh bounds | GPU id buffer (Unity scene picking, Unreal hit proxies) | Sponza is one model plus lights; an id pass is worth it only with many small entities. |
| Gizmos | ImGuizmo | Hand-written gizmos | Widely used, MIT, draws through ImGui; replace only if it limits us. |
| Fonts | Inter + JetBrains Mono + an icon font | ImGui's default ProggyClean; Roboto (Unreal) | Inter is built for UI at small sizes and is what Unity's editor uses; a mono font keeps numbers aligned. |
| Theme | Token table mapped to `ImGuiStyle` | Per-panel colors | One place to change the look; makes Darker/Light themes cheap. |

## Risks

- **ImGuizmo with reverse-Z infinite projection:** ImGuizmo assumes a conventional projection. The gizmo gets its
  own finite, standard-Z projection with the same FOV. Only the gizmo uses it.
- **Viewport resize churn:** dragging a dock splitter changes the size every frame. The texture is recreated at
  most once per frame and only when the size changes, and the deferred-release queue absorbs the old ones. If memory
  spikes, round sizes up to multiples of 64 and render with a smaller viewport rectangle.
- **Font blurriness at fractional DPI:** rasterize at the exact scaled size (never scale the atlas); fall back to
  `imgui_freetype` if needed.
- **Input conflicts:** ImGui, the camera and shortcuts all want the keyboard. The rule: text fields first, then the
  viewport camera while flying, then global shortcuts. Shortcuts are ignored while a text field is active.
- **Scope creep:** thumbnails, multi-viewport, an asset reimport UI and console variables are explicitly out of
  this phase.
