#ifndef ATMOS_COMMON_HLSLI
#define ATMOS_COMMON_HLSLI

// Bindings of the engine-wide root signature (graphics/d3d12/BindlessRootSignature.h):
//   b0 root constants (declared per shader), b1 view constants, b2 pass constants,
//   s0-s3 static samplers. Textures/buffers come from ResourceDescriptorHeap[index].
// Matrices are row-major (-Zpr): mul(vector, matrix).

// Mirrors atmos::ViewConstants (graphics/ShaderInterop.h).
struct ViewConstants
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float4x4 inverseViewProjection;
    float3 cameraPosition;
    uint frameNumber;
    float2 viewportSize;
    float2 inverseViewportSize;
};

ConstantBuffer<ViewConstants> View : register(b1);

SamplerState PointClamp : register(s0);
SamplerState LinearClamp : register(s1);
SamplerState LinearWrap : register(s2);
SamplerState AnisotropicWrap : register(s3);

struct FullscreenVertex
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// One oversized triangle covering the screen, placed on the reverse-Z far plane.
FullscreenVertex MakeFullscreenVertex(uint vertexId)
{
    FullscreenVertex output;
    output.uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(output.uv.x * 2.0 - 1.0, 1.0 - output.uv.y * 2.0, 0.0, 1.0);
    return output;
}

// World-space direction through a screen UV, unprojected from the near plane (depth 1).
float3 GetViewDirection(float2 uv)
{
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    float4 nearPoint = mul(float4(ndc, 1.0, 1.0), View.inverseViewProjection);
    return normalize(nearPoint.xyz / nearPoint.w - View.cameraPosition);
}

#endif
