#pragma once

#include "graphics/rendergraph/RenderGraph.h"

#include <DirectXMath.h>

#include <cstddef>
#include <cstdint>

namespace atmos
{
    // Clustered light grid: screen tiles x logarithmic depth slices (shaders/Lighting/LightClusters.hlsli).
    namespace LightGrid
    {
        inline constexpr uint32_t TilesX = 16;
        inline constexpr uint32_t TilesY = 9;
        inline constexpr uint32_t Slices = 24;
        inline constexpr uint32_t ClusterCount = TilesX * TilesY * Slices;
        inline constexpr uint32_t MaxLightsPerCluster = 128;
        inline constexpr float NearDepth = 0.1f;
        inline constexpr float FarDepth = 500.0f;
    }

    inline constexpr uint32_t NoShadowView = UINT32_MAX;

    // Mirrors LightData in shaders/Lighting/LightingCommon.hlsli.
    struct LightGpuData
    {
        DirectX::XMFLOAT3 position;
        float range;
        DirectX::XMFLOAT3 color;  // color * intensity
        uint32_t type;
        DirectX::XMFLOAT3 direction;
        float spotScale;
        float spotOffset;
        uint32_t shadowViewIndex;
        uint32_t padding[2];
    };

    static_assert(sizeof(LightGpuData) == 64);

    // Mirrors LightingConstants in shaders/Lighting/LightingCommon.hlsli.
    struct LightingConstants
    {
        DirectX::XMFLOAT3 sunDirection;
        float sunIntensity;
        DirectX::XMFLOAT3 sunColor;
        uint32_t sunShadowsEnabled;

        uint32_t lightBufferIndex;
        uint32_t lightCount;
        uint32_t clusterLightCountsIndex;
        uint32_t clusterLightIndicesIndex;

        uint32_t clusterGridSize[3];
        uint32_t maxLightsPerCluster;

        DirectX::XMFLOAT2 clusterTileSize;
        float clusterDepthScale;
        float clusterDepthBias;

        uint32_t viewBufferIndex;
        uint32_t cascadeCount;
        uint32_t cascadeShadowMapIndex;
        uint32_t shadowAtlasIndex;

        DirectX::XMFLOAT4 cascadeSplits;

        DirectX::XMFLOAT2 cascadeTexelSize;
        DirectX::XMFLOAT2 atlasTexelSize;

        float shadowNormalOffset;
        uint32_t localShadowsEnabled;
        uint32_t imageBasedLightingEnabled;
        float ambientIntensity;

        uint32_t irradianceSHIndex;
        uint32_t specularCubeIndex;
        uint32_t brdfLutIndex;
        float specularMipCount;

        uint32_t debugView;
        float padding[3];
    };

    static_assert(sizeof(LightingConstants) == 176);
    static_assert(offsetof(LightingConstants, cascadeSplits) == 96);
    static_assert(offsetof(LightingConstants, irradianceSHIndex) == 144);

    // Sky-light outputs read by every lit pass.
    struct SkyLightResources
    {
        RGBuffer irradianceSH;
        RGTexture specularCube;
        RGTexture brdfLut;
        uint32_t specularMipCount = 1;
    };

    // Graph resources a lit pass reads; invalid handles mean the feature is off this frame.
    struct LightingResources
    {
        RGTexture cascadeShadowMap;
        RGTexture shadowAtlas;
        RGBuffer clusterLightCounts;
        RGBuffer clusterLightIndices;
        SkyLightResources skyLight;
    };

    // Declares the reads of every valid lighting resource for a lit pass.
    void DeclareLightingReads(RGBuilder& builder, const LightingResources& resources);

    // Completes the CPU-built constants with the bindless indices of this frame's resources.
    LightingConstants ResolveLightingConstants(const RGContext& context, const LightingResources& resources,
                                               LightingConstants constants);
}
