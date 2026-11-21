#include "Lighting/SurfaceLighting.hlsli"
#include "Scene/GBuffer.hlsli"

// Lights every GBuffer pixel: sun with cascaded shadows, clustered local lights, sky lighting.
// Adds into scene color, which already holds emissive; sky pixels (depth 0) are left untouched.

// Mirrors DeferredLightingConstants in DeferredLightingPass.cpp.
struct DeferredLightingConstants
{
    uint depthIndex;
    uint gbufferAIndex;
    uint gbufferBIndex;
    uint gbufferCIndex;
    uint sceneColorUav;
};

ConstantBuffer<DeferredLightingConstants> Constants : register(b0);

float3 HeatMap(float t)
{
    t = saturate(t);
    return saturate(float3(t * 2.0 - 0.5, t < 0.5 ? t * 2.0 : 2.0 - t * 2.0, 1.0 - t * 2.0));
}

float3 GetDebugColor(GBufferData gbuffer, Surface surface, float2 pixel, float viewDepth)
{
    switch (Lighting.debugView)
    {
        case DebugViewBaseColor:        return gbuffer.baseColor;
        case DebugViewNormals:          return gbuffer.normal * 0.5 + 0.5;
        case DebugViewRoughness:        return gbuffer.roughness;
        case DebugViewMetallic:         return gbuffer.metallic;
        case DebugViewAmbientOcclusion: return gbuffer.occlusion;
        case DebugViewShadowCascades:
        {
            static const float3 cascadeColors[4] = { float3(1, 0.3, 0.3), float3(0.3, 1, 0.3), float3(0.3, 0.3, 1), float3(1, 1, 0.3) };
            float shadow = SampleSunShadow(surface.position, surface.normal, viewDepth);
            float3 tint = viewDepth <= Lighting.cascadeSplits[Lighting.cascadeCount - 1] ? cascadeColors[SelectCascade(viewDepth)] : 0.5;
            return tint * lerp(0.2, 1.0, shadow);
        }
        case DebugViewLightComplexity:
        {
            uint clusterIndex = GetClusterIndex(GetClusterCoordinate(pixel, viewDepth));
            return HeatMap(GetClusterLightCount(clusterIndex) / 32.0);
        }
        default:
            return 0.0;
    }
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= (uint2)View.viewportSize))
        return;

    Texture2D<float> depthTexture = ResourceDescriptorHeap[Constants.depthIndex];
    float depth = depthTexture[id.xy];

    if (depth <= 0.0)
        return;

    Texture2D<float4> gbufferA = ResourceDescriptorHeap[Constants.gbufferAIndex];
    Texture2D<float4> gbufferB = ResourceDescriptorHeap[Constants.gbufferBIndex];
    Texture2D<float4> gbufferC = ResourceDescriptorHeap[Constants.gbufferCIndex];
    GBufferData gbuffer = DecodeGBuffer(gbufferA[id.xy], gbufferB[id.xy], gbufferC[id.xy]);

    float2 pixel = id.xy + 0.5;
    float3 worldPosition = ReconstructWorldPosition(pixel * View.inverseViewportSize, depth);
    float viewDepth = GetViewDepth(worldPosition);
    Surface surface = MakeSurface(worldPosition, gbuffer.normal, gbuffer.baseColor, gbuffer.metallic, gbuffer.roughness,
                                  gbuffer.occlusion);

    RWTexture2D<float4> sceneColor = ResourceDescriptorHeap[Constants.sceneColorUav];

    if (Lighting.debugView != DebugViewLit)
    {
        sceneColor[id.xy] = float4(GetDebugColor(gbuffer, surface, pixel, viewDepth), 1.0);
        return;
    }

    if (gbuffer.shadingModel == ShadingModelUnlit)
        return;

    sceneColor[id.xy] += float4(EvaluateLighting(surface, pixel, viewDepth), 0.0);
}
