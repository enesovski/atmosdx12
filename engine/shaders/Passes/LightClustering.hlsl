#include "Lighting/LightClusters.hlsli"

// Builds the clustered light grid: one thread group per cluster tests every light's bounding
// sphere against the cluster's view-space box and appends the indices that overlap.

// Mirrors ClusteringConstants in LightClusteringPass.cpp.
struct ClusteringConstants
{
    uint lightCountsUav;
    uint lightIndicesUav;
};

ConstantBuffer<ClusteringConstants> Constants : register(b0);

static const uint ThreadCount = 64;

groupshared float3 ClusterMin;
groupshared float3 ClusterMax;
groupshared uint ClusterLightCount;

// View-space point on the ray through `ndc` at `viewDepth` (row-vector projection).
float3 GetViewPosition(float2 ndc, float viewDepth)
{
    return float3(ndc.x * viewDepth / View.projection[0][0], ndc.y * viewDepth / View.projection[1][1], viewDepth);
}

void ComputeClusterBounds(uint3 coordinate)
{
    float2 pixelMin = coordinate.xy * Lighting.clusterTileSize;
    float2 pixelMax = min(pixelMin + Lighting.clusterTileSize, View.viewportSize);
    float2 ndcMin = float2(pixelMin.x * View.inverseViewportSize.x * 2.0 - 1.0, 1.0 - pixelMax.y * View.inverseViewportSize.y * 2.0);
    float2 ndcMax = float2(pixelMax.x * View.inverseViewportSize.x * 2.0 - 1.0, 1.0 - pixelMin.y * View.inverseViewportSize.y * 2.0);

    float nearDepth = exp((coordinate.z - Lighting.clusterDepthBias) / Lighting.clusterDepthScale);
    float farDepth = exp((coordinate.z + 1.0 - Lighting.clusterDepthBias) / Lighting.clusterDepthScale);

    float3 corners[8] = {
        GetViewPosition(ndcMin, nearDepth), GetViewPosition(float2(ndcMax.x, ndcMin.y), nearDepth),
        GetViewPosition(float2(ndcMin.x, ndcMax.y), nearDepth), GetViewPosition(ndcMax, nearDepth),
        GetViewPosition(ndcMin, farDepth), GetViewPosition(float2(ndcMax.x, ndcMin.y), farDepth),
        GetViewPosition(float2(ndcMin.x, ndcMax.y), farDepth), GetViewPosition(ndcMax, farDepth),
    };

    float3 boundsMin = corners[0];
    float3 boundsMax = corners[0];

    [unroll]
    for (uint i = 1; i < 8; ++i)
    {
        boundsMin = min(boundsMin, corners[i]);
        boundsMax = max(boundsMax, corners[i]);
    }

    ClusterMin = boundsMin;
    ClusterMax = boundsMax;
    ClusterLightCount = 0;
}

[numthreads(ThreadCount, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint threadIndex : SV_GroupIndex)
{
    uint clusterIndex = groupId.x;
    uint3 grid = Lighting.clusterGridSize;
    uint3 coordinate = uint3(clusterIndex % grid.x, (clusterIndex / grid.x) % grid.y, clusterIndex / (grid.x * grid.y));

    if (threadIndex == 0)
        ComputeClusterBounds(coordinate);

    GroupMemoryBarrierWithGroupSync();

    RWStructuredBuffer<uint> lightIndices = ResourceDescriptorHeap[Constants.lightIndicesUav];

    for (uint lightIndex = threadIndex; lightIndex < Lighting.lightCount; lightIndex += ThreadCount)
    {
        LightData light = LoadLight(lightIndex);
        float3 center = mul(float4(light.position, 1.0), View.view).xyz;
        float3 offset = clamp(center, ClusterMin, ClusterMax) - center;

        if (dot(offset, offset) <= light.range * light.range)
        {
            uint slot;
            InterlockedAdd(ClusterLightCount, 1, slot);

            if (slot < Lighting.maxLightsPerCluster)
                lightIndices[clusterIndex * Lighting.maxLightsPerCluster + slot] = lightIndex;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (threadIndex == 0)
    {
        RWStructuredBuffer<uint> lightCounts = ResourceDescriptorHeap[Constants.lightCountsUav];
        lightCounts[clusterIndex] = ClusterLightCount;
    }
}
