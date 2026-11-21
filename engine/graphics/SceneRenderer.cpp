#include "graphics/SceneRenderer.h"
#include "graphics/Frustum.h"
#include "graphics/RenderScene.h"
#include "graphics/RenderView.h"
#include "graphics/d3d12/GraphicsConfig.h"

using namespace DirectX;

namespace atmos
{
    SceneRenderer::SceneRenderer(GraphicsDevice& device, const ShaderCompiler& compiler)
        : gpuScene_(device),
          viewBuffer_(device, LightSetup::MaxViews, L"View Data"),
          lightBuffer_(device, LightSetup::MaxLights, L"Light Data"),
          skyLight_(device, compiler),
          shadowPass_(device, compiler),
          depthPrepass_(device, compiler),
          gbufferPass_(device, compiler),
          lightClusteringPass_(device, compiler),
          deferredLightingPass_(device, compiler),
          skyPass_(device, compiler),
          transparentPass_(device, compiler),
          tonemapPass_(device, compiler, GraphicsConfig::BackBufferFormat),
          editorOverlayPass_(device, compiler, GraphicsConfig::BackBufferFormat)
    {
    }

    void SceneRenderer::Prepare(uint32_t frameIndex, const RenderScene& scene, const RenderView& view, uint32_t width,
                                uint32_t height)
    {
        width_ = width;
        height_ = height;

        gpuScene_.Update(frameIndex, scene);
        lightSetup_.Prepare(scene, view);

        viewBuffer_.Write(frameIndex, lightSetup_.GetViews());
        lightBuffer_.Write(frameIndex, lightSetup_.GetLights());

        const XMMATRIX viewProjection = XMLoadFloat4x4(&view.viewMatrix) * XMLoadFloat4x4(&view.projectionMatrix);
        const Frustum cameraFrustum = Frustum::FromViewProjection(viewProjection);
        gpuScene_.Cull(cameraFrustum, cameraDrawList_, true);
        gpuScene_.SortTransparentBackToFront(cameraDrawList_, view.position);
        gpuScene_.CullEntities(cameraFrustum, scene.overlays.selectedEntities, selectionDrawList_);

        const std::vector<ShadowView>& shadowViews = lightSetup_.GetShadowViews();
        shadowDrawLists_.resize(shadowViews.size());
        stats_.shadowDraws = 0;

        for (size_t i = 0; i < shadowViews.size(); ++i)
        {
            gpuScene_.Cull(shadowViews[i].frustum, shadowDrawLists_[i], false);
            stats_.shadowDraws += shadowDrawLists_[i].GetDrawCount();
        }

        lightingConstants_ = lightSetup_.BuildConstants(scene, width, height);
        lightingConstants_.viewBufferIndex = viewBuffer_.GetSrvIndex(frameIndex);
        lightingConstants_.lightBufferIndex = lightBuffer_.GetSrvIndex(frameIndex);

        stats_.scene = gpuScene_.GetStats();
        stats_.visibleDraws = cameraDrawList_.GetDrawCount();
        stats_.lights = static_cast<uint32_t>(lightSetup_.GetLights().size());
        stats_.shadowViews = static_cast<uint32_t>(shadowViews.size());
        stats_.shadowedLights = 0;

        for (const LightGpuData& light : lightSetup_.GetLights())
            stats_.shadowedLights += light.shadowViewIndex != NoShadowView;
    }

    void SceneRenderer::AddPasses(RenderGraph& graph, const RenderScene& scene, RGTexture output)
    {
        const uint32_t viewBufferIndex = lightingConstants_.viewBufferIndex;

        const RGTexture depth = graph.CreateTexture({
            .width = width_,
            .height = height_,
            .format = GraphicsConfig::DepthFormat,
            .debugName = L"Scene Depth",
        });

        const RGTexture sceneColor = graph.CreateTexture({
            .width = width_,
            .height = height_,
            .format = GraphicsConfig::HdrColorFormat,
            .debugName = L"Scene Color",
        });

        LightingResources lighting;
        lighting.skyLight = skyLight_.AddToGraph(graph, scene.sky);

        shadowPass_.AddToGraph(graph, gpuScene_, lightSetup_.GetShadowViews(), shadowDrawLists_, viewBufferIndex,
                               lightSetup_.GetCascadeResolution(), lighting.cascadeShadowMap, lighting.shadowAtlas);

        depthPrepass_.AddToGraph(graph, gpuScene_, cameraDrawList_, viewBufferIndex, depth);
        const GBufferTargets gbuffer =
            gbufferPass_.AddToGraph(graph, gpuScene_, cameraDrawList_, viewBufferIndex, depth, sceneColor, width_, height_);

        lightClusteringPass_.AddToGraph(graph, lightingConstants_, lighting);
        deferredLightingPass_.AddToGraph(graph, lightingConstants_, lighting, depth, gbuffer, sceneColor, width_, height_);

        skyPass_.AddToGraph(graph, scene.sky, depth, sceneColor);
        transparentPass_.AddToGraph(graph, gpuScene_, cameraDrawList_, viewBufferIndex, lightingConstants_, lighting, depth,
                                    sceneColor);

        const DebugView debugView = scene.settings.debugView;
        const bool showsRawValues = debugView != DebugView::Lit && debugView != DebugView::LightingOnly;
        tonemapPass_.AddToGraph(graph, scene.postProcess, sceneColor, output, showsRawValues);

        editorOverlayPass_.AddToGraph(graph, gpuScene_, scene.overlays, cameraDrawList_, selectionDrawList_,
                                      viewBufferIndex, depth, output, width_, height_);
    }
}
