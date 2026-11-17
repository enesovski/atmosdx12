#include "Common.hlsli"

// Mirrors TonemapConstants in TonemapPass.cpp.
struct TonemapConstants
{
    uint hdrTextureIndex;
    float exposure;
};

ConstantBuffer<TonemapConstants> Constants : register(b0);

FullscreenVertex VSMain(uint vertexId : SV_VertexID)
{
    return MakeFullscreenVertex(vertexId);
}

float3 LinearToSRGB(float3 color)
{
    float3 low = color * 12.92;
    float3 high = 1.055 * pow(max(color, 0.0), 1.0 / 2.4) - 0.055;
    return lerp(low, high, step(0.0031308, color));
}

float4 PSMain(FullscreenVertex input) : SV_Target
{
    Texture2D<float4> hdrTexture = ResourceDescriptorHeap[Constants.hdrTextureIndex];

    // Input and output share a resolution, so load the matching texel directly.
    float3 hdrColor = hdrTexture.Load(int3(input.position.xy, 0)).rgb;
    float3 mapped = 1.0 - exp(-hdrColor * Constants.exposure);

    return float4(LinearToSRGB(saturate(mapped)), 1.0);
}
