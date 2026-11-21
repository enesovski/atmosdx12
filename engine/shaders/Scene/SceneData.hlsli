#ifndef ATMOS_SCENE_DATA_HLSLI
#define ATMOS_SCENE_DATA_HLSLI

#include "Common.hlsli"
#include "Scene/ViewData.hlsli"

// Per-draw data written by GpuScene (graphics/GpuScene.h) and fetched through bindless indices.

// Mirrors MeshRootConstants in graphics/GpuScene.h.
struct MeshRootConstants
{
    uint drawIndex;
    uint drawBufferIndex;
    uint materialBufferIndex;
    uint positionBufferIndex;
    uint attributeBufferIndex;
    uint viewBufferIndex;
    uint viewIndex;
};

// Mirrors DrawData in graphics/GpuScene.h.
struct DrawData
{
    float4x4 world;
    float4x4 normalMatrix;
    uint materialIndex;
    uint3 padding;
};

// Mirrors MaterialGpuData in graphics/Material.h.
struct MaterialData
{
    float4 baseColorFactor;
    float3 emissiveFactor;
    float alphaCutoff;
    float metallicFactor;
    float roughnessFactor;
    uint baseColorTexture;
    uint normalTexture;
    uint metallicRoughnessTexture;
    uint occlusionTexture;
    uint emissiveTexture;
    uint padding;
};

ConstantBuffer<MeshRootConstants> Draw : register(b0);

static const uint PositionStride = 12;   // float3
static const uint AttributeStride = 36;  // float3 normal + float4 tangent + float2 uv

struct VertexAttributes
{
    float3 normal;
    float4 tangent;
    float2 uv;
};

DrawData LoadDrawData()
{
    StructuredBuffer<DrawData> draws = ResourceDescriptorHeap[Draw.drawBufferIndex];
    return draws[Draw.drawIndex];
}

MaterialData LoadMaterial(uint materialIndex)
{
    StructuredBuffer<MaterialData> materials = ResourceDescriptorHeap[Draw.materialBufferIndex];
    return materials[materialIndex];
}

float3 LoadPosition(uint vertexId)
{
    ByteAddressBuffer positions = ResourceDescriptorHeap[Draw.positionBufferIndex];
    return asfloat(positions.Load3(vertexId * PositionStride));
}

VertexAttributes LoadAttributes(uint vertexId)
{
    ByteAddressBuffer attributes = ResourceDescriptorHeap[Draw.attributeBufferIndex];
    uint offset = vertexId * AttributeStride;

    VertexAttributes result;
    result.normal = asfloat(attributes.Load3(offset));
    result.tangent = asfloat(attributes.Load4(offset + 12));
    result.uv = asfloat(attributes.Load2(offset + 28));
    return result;
}

// Every pass of the same view must produce bit-identical depth (EQUAL test after the prepass).
float4 TransformToClip(float3 localPosition, float4x4 world)
{
    ViewData view = LoadView(Draw.viewBufferIndex, Draw.viewIndex);
    precise float4 worldPosition = mul(float4(localPosition, 1.0), world);
    precise float4 clipPosition = mul(worldPosition, view.viewProjection);
    return clipPosition;
}

float4 SampleMaterialTexture(uint textureIndex, float2 uv)
{
    Texture2D<float4> texture = ResourceDescriptorHeap[textureIndex];
    return texture.Sample(AnisotropicWrap, uv);
}

#endif
