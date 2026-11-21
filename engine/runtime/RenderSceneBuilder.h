#pragma once

namespace atmos
{
    class AssetManager;
    class Scene;
    struct RenderScene;

    // Starts loading every model the scene references that has no handle yet.
    void RequestSceneAssets(Scene& scene, AssetManager& assets);

    // Rebuilds `out` from the scene (including its render settings), skipping inactive entities and
    // models that are not ready yet. Viewport state (debug view, overlays) is left to the caller.
    void BuildRenderScene(const Scene& scene, const AssetManager& assets, RenderScene& out);
}
