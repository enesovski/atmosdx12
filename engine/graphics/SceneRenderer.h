#pragma once

#include "graphics/GpuScene.h"
#include "graphics/lighting/LightSetup.h"
#include "graphics/lighting/SkyLight.h"
#include "graphics/passes/DeferredLightingPass.h"
#include "graphics/passes/DepthPrepass.h"
#include "graphics/passes/GBufferPass.h"
#include "graphics/passes/LightClusteringPass.h"
#include "graphics/passes/ShadowPass.h"
#include "graphics/passes/SkyPass.h"
#include "graphics/passes/TonemapPass.h"
#include "graphics/passes/TransparentPass.h"

#include <cstdint>
#include <vector>

namespace atmos
{
    struct RenderScene;
    struct RenderView;

    struct SceneRenderStats
    {
        GpuSceneStats scene;
        uint32_t visibleDraws = 0;
        uint32_t lights = 0;
        uint32_t shadowedLights = 0;
        uint32_t shadowViews = 0;
        uint32_t shadowDraws = 0;
    };

    // Renders the scene for one camera, deferred (Unreal's DeferredShadingSceneRenderer):
    //   Prepare()   CPU: draw data, lights, shadow views, per-view culling, uploads
    //   AddPasses() GPU: shadows, depth prepass, GBuffer, light grid, lighting, sky, transparents, tonemap
    class SceneRenderer
    {
    public:
        SceneRenderer(GraphicsDevice& device, const ShaderCompiler& compiler);

        void Prepare(uint32_t frameIndex, const RenderScene& scene, const RenderView& view, uint32_t width,
                     uint32_t height);
        void AddPasses(RenderGraph& graph, const RenderScene& scene, RGTexture output);

        const SceneRenderStats& GetStats() const { return stats_; }

    private:
        GpuScene gpuScene_;
        LightSetup lightSetup_;
        FrameStructuredBuffer<ViewData> viewBuffer_;
        FrameStructuredBuffer<LightGpuData> lightBuffer_;
        SkyLight skyLight_;

        ShadowPass shadowPass_;
        DepthPrepass depthPrepass_;
        GBufferPass gbufferPass_;
        LightClusteringPass lightClusteringPass_;
        DeferredLightingPass deferredLightingPass_;
        SkyPass skyPass_;
        TransparentPass transparentPass_;
        TonemapPass tonemapPass_;

        DrawList cameraDrawList_;
        std::vector<DrawList> shadowDrawLists_;
        LightingConstants lightingConstants_{};
        SceneRenderStats stats_;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
    };
}
