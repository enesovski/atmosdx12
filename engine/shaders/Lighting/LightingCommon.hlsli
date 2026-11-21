#ifndef ATMOS_LIGHTING_COMMON_HLSLI
#define ATMOS_LIGHTING_COMMON_HLSLI

#include "Common.hlsli"

// Mirrors LightingConstants in graphics/lighting/LightingTypes.h. Bound as the pass CBV (b2) by
// every pass that shades surfaces or builds the light grid.
struct LightingConstants
{
    float3 sunDirection;               // toward the sun
    float sunIntensity;
    float3 sunColor;
    uint sunShadowsEnabled;

    uint lightBufferIndex;
    uint lightCount;
    uint clusterLightCountsIndex;
    uint clusterLightIndicesIndex;

    uint3 clusterGridSize;
    uint maxLightsPerCluster;

    float2 clusterTileSize;            // pixels per cluster in x and y
    float clusterDepthScale;           // slice = log(viewDepth) * scale + bias
    float clusterDepthBias;

    uint viewBufferIndex;
    uint cascadeCount;
    uint cascadeShadowMapIndex;
    uint shadowAtlasIndex;

    float4 cascadeSplits;              // view depth where each cascade ends

    float2 cascadeTexelSize;
    float2 atlasTexelSize;

    float shadowNormalOffset;          // in shadow-map texels
    uint localShadowsEnabled;
    uint imageBasedLightingEnabled;
    float ambientIntensity;

    uint irradianceSHIndex;
    uint specularCubeIndex;
    uint brdfLutIndex;
    float specularMipCount;

    uint debugView;
    float3 padding;
};

ConstantBuffer<LightingConstants> Lighting : register(b2);

// Mirrors LightGpuData in graphics/lighting/LightingTypes.h.
static const uint LightTypePoint = 0;
static const uint LightTypeSpot = 1;
static const uint NoShadow = 0xFFFFFFFF;

struct LightData
{
    float3 position;
    float range;
    float3 color;                      // color * intensity
    uint type;
    float3 direction;                  // spot axis
    float spotScale;
    float spotOffset;
    uint shadowViewIndex;              // first shadow view in the view buffer, or NoShadow
    uint2 padding;
};

// Mirrors DebugView in graphics/RenderSettings.h.
static const uint DebugViewLit = 0;
static const uint DebugViewBaseColor = 1;
static const uint DebugViewNormals = 2;
static const uint DebugViewRoughness = 3;
static const uint DebugViewMetallic = 4;
static const uint DebugViewAmbientOcclusion = 5;
static const uint DebugViewShadowCascades = 6;
static const uint DebugViewLightComplexity = 7;

// A lit surface point, independent of where its data came from (GBuffer or forward material).
struct Surface
{
    float3 position;
    float3 normal;
    float3 view;           // toward the eye
    float3 diffuseColor;   // base color * (1 - metallic)
    float3 f0;             // specular reflectance at normal incidence
    float roughness;
    float occlusion;
};

Surface MakeSurface(float3 position, float3 normal, float3 baseColor, float metallic, float roughness, float occlusion)
{
    Surface surface;
    surface.position = position;
    surface.normal = normal;
    surface.view = normalize(View.cameraPosition - position);
    surface.diffuseColor = baseColor * (1.0 - metallic);
    surface.f0 = lerp(0.04, baseColor, metallic);
    surface.roughness = max(roughness, 0.045);
    surface.occlusion = occlusion;
    return surface;
}

LightData LoadLight(uint lightIndex)
{
    StructuredBuffer<LightData> lights = ResourceDescriptorHeap[Lighting.lightBufferIndex];
    return lights[lightIndex];
}

#endif
