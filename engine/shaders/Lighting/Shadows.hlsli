#ifndef ATMOS_SHADOWS_HLSLI
#define ATMOS_SHADOWS_HLSLI

#include "Lighting/LightingCommon.hlsli"
#include "Scene/ViewData.hlsli"

// Shadow maps use reverse-Z like the camera: a receiver is lit when its depth >= the stored
// occluder depth (ShadowCompare, hardware 2x2 PCF). Filtering is a 3x3 tent of PCF taps, or a
// single tap for ShadowFilterHard.

float FilterShadow3x3(Texture2DArray<float> map, float2 uv, float slice, float depth, float2 texelSize)
{
    if (Lighting.shadowFilter == ShadowFilterHard)
        return map.SampleCmpLevelZero(ShadowCompare, float3(uv, slice), depth);

    float sum = 0.0;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
            sum += map.SampleCmpLevelZero(ShadowCompare, float3(uv + float2(x, y) * texelSize, slice), depth);
    }

    return sum / 9.0;
}

float FilterShadow3x3(Texture2D<float> map, float2 uv, float depth, float2 texelSize, float2 tileMin, float2 tileMax)
{
    if (Lighting.shadowFilter == ShadowFilterHard)
        return map.SampleCmpLevelZero(ShadowCompare, clamp(uv, tileMin, tileMax), depth);

    float sum = 0.0;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
            sum += map.SampleCmpLevelZero(ShadowCompare, clamp(uv + float2(x, y) * texelSize, tileMin, tileMax), depth);
    }

    return sum / 9.0;
}

float3 ProjectToShadowMap(ViewData shadowView, float3 worldPosition)
{
    float4 clip = mul(float4(worldPosition, 1.0), shadowView.viewProjection);
    float3 ndc = clip.xyz / clip.w;
    return float3(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5, ndc.z);
}

uint SelectCascade(float viewDepth)
{
    uint cascade = 0;

    [unroll]
    for (uint i = 0; i < 3; ++i)
        cascade += (i + 1 < Lighting.cascadeCount && viewDepth > Lighting.cascadeSplits[i]) ? 1 : 0;

    return cascade;
}

// Sun visibility. The receiver is pushed along its normal by a few cascade texels (normal-offset
// bias) so slopes do not self-shadow; the shadow pass adds slope-scaled depth bias on top.
float SampleSunShadow(float3 worldPosition, float3 normal, float viewDepth)
{
    if (!Lighting.sunShadowsEnabled)
        return 1.0;

    if (viewDepth > Lighting.cascadeSplits[Lighting.cascadeCount - 1])
        return 1.0;

    uint cascade = SelectCascade(viewDepth);
    ViewData shadowView = LoadView(Lighting.viewBufferIndex, 1 + cascade);

    float3 offsetPosition = worldPosition + normal * shadowView.texelWorldSize * Lighting.shadowNormalOffset;
    float3 shadowCoord = ProjectToShadowMap(shadowView, offsetPosition);

    Texture2DArray<float> cascades = ResourceDescriptorHeap[Lighting.cascadeShadowMapIndex];
    return FilterShadow3x3(cascades, shadowCoord.xy, cascade, shadowCoord.z, Lighting.cascadeTexelSize);
}

// Point lights use six cube-face views, picked by the dominant axis of the light-to-surface vector.
uint GetPointShadowFace(float3 lightToSurface)
{
    float3 a = abs(lightToSurface);

    if (a.x >= a.y && a.x >= a.z)
        return lightToSurface.x >= 0.0 ? 0 : 1;

    if (a.y >= a.z)
        return lightToSurface.y >= 0.0 ? 2 : 3;

    return lightToSurface.z >= 0.0 ? 4 : 5;
}

float SampleLocalShadow(LightData light, float3 worldPosition, float3 normal)
{
    if (!Lighting.localShadowsEnabled || light.shadowViewIndex == NoShadow)
        return 1.0;

    uint viewIndex = light.shadowViewIndex;

    if (light.type == LightTypePoint)
        viewIndex += GetPointShadowFace(worldPosition - light.position);

    ViewData shadowView = LoadView(Lighting.viewBufferIndex, viewIndex);
    float distanceToLight = length(worldPosition - light.position);
    float3 offsetPosition = worldPosition + normal * shadowView.texelWorldSize * distanceToLight * Lighting.shadowNormalOffset;
    float3 shadowCoord = ProjectToShadowMap(shadowView, offsetPosition);

    if (any(shadowCoord.xy < 0.0) || any(shadowCoord.xy > 1.0))
        return 1.0;

    // Map the tile-local uv into the atlas and keep the PCF footprint inside the tile.
    float2 scale = shadowView.atlasScaleOffset.xy;
    float2 offset = shadowView.atlasScaleOffset.zw;
    float2 atlasUv = shadowCoord.xy * scale + offset;
    float2 halfTexel = 0.5 * Lighting.atlasTexelSize;

    Texture2D<float> atlas = ResourceDescriptorHeap[Lighting.shadowAtlasIndex];
    return FilterShadow3x3(atlas, atlasUv, shadowCoord.z, Lighting.atlasTexelSize, offset + halfTexel,
                           offset + scale - halfTexel);
}

#endif
