#ifndef ATMOS_LIGHT_CLUSTERS_HLSLI
#define ATMOS_LIGHT_CLUSTERS_HLSLI

#include "Lighting/LightingCommon.hlsli"

// Clustered light grid (Olsson 2012): screen tiles x logarithmic view-depth slices. Each cluster
// stores up to maxLightsPerCluster light indices (filled by LightClustering.hlsl).

uint3 GetClusterCoordinate(float2 pixel, float viewDepth)
{
    uint slice = (uint)max(log(max(viewDepth, 1e-4)) * Lighting.clusterDepthScale + Lighting.clusterDepthBias, 0.0);
    uint2 tile = (uint2)(pixel / Lighting.clusterTileSize);
    return min(uint3(tile, slice), Lighting.clusterGridSize - 1);
}

uint GetClusterIndex(uint3 coordinate)
{
    return coordinate.x + Lighting.clusterGridSize.x * (coordinate.y + Lighting.clusterGridSize.y * coordinate.z);
}

uint GetClusterLightCount(uint clusterIndex)
{
    StructuredBuffer<uint> counts = ResourceDescriptorHeap[Lighting.clusterLightCountsIndex];
    return min(counts[clusterIndex], Lighting.maxLightsPerCluster);
}

uint GetClusterLightIndex(uint clusterIndex, uint i)
{
    StructuredBuffer<uint> indices = ResourceDescriptorHeap[Lighting.clusterLightIndicesIndex];
    return indices[clusterIndex * Lighting.maxLightsPerCluster + i];
}

#endif
