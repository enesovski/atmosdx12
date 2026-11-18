#pragma once

namespace atmos
{
    class AssetManager;
    class Scene;
    struct RenderScene;

    // Starts loading every model the scene references that has no handle yet.
    void RequestSceneAssets(Scene& scene, AssetManager& assets);

    // Rebuilds `out` from the scene: environment settings plus one MeshInstance per model
    // node, for every model that is ready. `out` keeps its capacity between frames.
    void BuildRenderScene(const Scene& scene, const AssetManager& assets, RenderScene& out);
}
