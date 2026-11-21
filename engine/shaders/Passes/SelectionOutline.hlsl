#include "Common.hlsli"

// Draws an outline around the selection mask (Unity/Unreal-style), plus a faint tint inside it.
// The whole silhouette is outlined, including parts hidden behind other geometry.

// Mirrors OutlineConstants in EditorOverlayPass.cpp.
struct OutlineConstants
{
    float4 color;
    uint maskIndex;
};

ConstantBuffer<OutlineConstants> Constants : register(b0);

static const int Radius = 2;

FullscreenVertex VSMain(uint vertexId : SV_VertexID)
{
    return MakeFullscreenVertex(vertexId);
}

float4 PSMain(FullscreenVertex input) : SV_Target
{
    Texture2D<float> mask = ResourceDescriptorHeap[Constants.maskIndex];
    int2 pixel = int2(input.position.xy);
    float center = mask.Load(int3(pixel, 0));

    // Out-of-range loads return 0, so the screen border needs no clamping.
    float neighbor = 0.0;

    [unroll]
    for (int y = -Radius; y <= Radius; ++y)
    {
        [unroll]
        for (int x = -Radius; x <= Radius; ++x)
        {
            if (x * x + y * y <= Radius * Radius + 1)
                neighbor = max(neighbor, mask.Load(int3(pixel + int2(x, y), 0)));
        }
    }

    float edge = neighbor * (1.0 - center);
    float alpha = max(edge, center * 0.08);
    return float4(Constants.color.rgb, Constants.color.a * alpha);
}
