#include "Common.hlsli"
#include "Sky/SkyModel.hlsli"

// Mirrors SkyConstants in SkyPass.cpp.
struct SkyConstants
{
    float3 sunDirection;
    float sunDiskIntensity;
};

ConstantBuffer<SkyConstants> Constants : register(b0);

FullscreenVertex VSMain(uint vertexId : SV_VertexID)
{
    return MakeFullscreenVertex(vertexId);
}

float4 PSMain(FullscreenVertex input) : SV_Target
{
    float3 viewDirection = GetViewDirection(input.uv);
    float3 color = GetSkyRadiance(viewDirection, Constants.sunDirection) +
                   GetSunDiskRadiance(viewDirection, Constants.sunDirection, Constants.sunDiskIntensity);
    return float4(color, 1.0);
}
