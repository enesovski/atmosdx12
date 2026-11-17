#include "Common.hlsli"

// Shared by DepthPrepass (VSDepth) and OpaquePass (VSMain + PSMain).
// Vertices are pulled from bindless ByteAddressBuffers; there is no input layout.

// Mirrors MeshRootConstants in graphics/GpuScene.h.
struct MeshRootConstants
{
    uint drawIndex;
    uint drawBufferIndex;
    uint positionBufferIndex;
    uint attributeBufferIndex;
};

// Mirrors DrawData in graphics/GpuScene.h.
struct DrawData
{
    float4x4 world;
    float4x4 normalMatrix;
    float4 baseColor;
};

// Mirrors OpaquePassConstants in OpaquePass.cpp.
struct OpaquePassConstants
{
    float3 sunDirection;
    float sunIntensity;
    float3 sunColor;
    float padding0;
    float3 ambientColor;
    float padding1;
};

ConstantBuffer<MeshRootConstants> Root : register(b0);
ConstantBuffer<OpaquePassConstants> Pass : register(b2);

static const uint PositionStride = 12;   // float3
static const uint AttributeStride = 20;  // float3 normal + float2 uv

DrawData LoadDrawData()
{
    StructuredBuffer<DrawData> draws = ResourceDescriptorHeap[Root.drawBufferIndex];
    return draws[Root.drawIndex];
}

// Both passes must produce bit-identical depth for the opaque EQUAL test.
float4 TransformToClip(uint vertexId, float4x4 world)
{
    ByteAddressBuffer positions = ResourceDescriptorHeap[Root.positionBufferIndex];
    float3 localPosition = asfloat(positions.Load3(vertexId * PositionStride));

    precise float4 worldPosition = mul(float4(localPosition, 1.0), world);
    precise float4 clipPosition = mul(worldPosition, View.viewProjection);
    return clipPosition;
}

float4 VSDepth(uint vertexId : SV_VertexID) : SV_Position
{
    return TransformToClip(vertexId, LoadDrawData().world);
}

struct OpaqueVertex
{
    float4 position : SV_Position;
    float3 worldNormal : NORMAL;
    nointerpolation float4 baseColor : COLOR;
};

OpaqueVertex VSMain(uint vertexId : SV_VertexID)
{
    DrawData draw = LoadDrawData();

    ByteAddressBuffer attributes = ResourceDescriptorHeap[Root.attributeBufferIndex];
    float3 normal = asfloat(attributes.Load3(vertexId * AttributeStride));

    OpaqueVertex output;
    output.position = TransformToClip(vertexId, draw.world);
    output.worldNormal = mul(float4(normal, 0.0), draw.normalMatrix).xyz;
    output.baseColor = draw.baseColor;
    return output;
}

// Temporary lighting until the deferred PBR path (Phase 3): lambert sun + flat ambient.
float4 PSMain(OpaqueVertex input) : SV_Target
{
    float3 normal = normalize(input.worldNormal);
    float sunAmount = saturate(dot(normal, Pass.sunDirection));

    float3 lighting = Pass.ambientColor + Pass.sunColor * Pass.sunIntensity * sunAmount;
    return float4(input.baseColor.rgb * lighting, 1.0);
}
