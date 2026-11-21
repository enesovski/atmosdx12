#pragma once

namespace atmos
{
    class AssetManager;
    class Scene;
    struct RenderScene;

    // Starts loading every model the scene references that has no handle yet.
    void RequestSceneAssets(Scene& scene, AssetManager& assets);

    // Rebuilds `out` from the scene, skipping models that are not ready yet.
    void BuildRenderScene(const Scene& scene, const AssetManager& assets, RenderScene& out);
}
