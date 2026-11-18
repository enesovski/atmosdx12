#include "Common.hlsli"

// Shared by DepthPrepass (VSDepth, VSDepthMasked + PSDepthMasked) and OpaquePass (VSMain + PSMain).
// Vertices are pulled from bindless ByteAddressBuffers; there is no input layout.

// Mirrors MeshRootConstants in graphics/GpuScene.h.
struct MeshRootConstants
{
    uint drawIndex;
    uint drawBufferIndex;
    uint materialBufferIndex;
    uint positionBufferIndex;
    uint attributeBufferIndex;
};

// Mirrors DrawData in graphics/GpuScene.h.
struct DrawData
{
    float4x4 world;
    float4x4 normalMatrix;
    uint materialIndex;
    uint3 padding;
};

// Mirrors MaterialGpuData in graphics/Material.h (texture fields are bindless indices).
struct MaterialGpuData
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
static const uint AttributeStride = 36;  // float3 normal + float4 tangent + float2 uv

DrawData LoadDrawData()
{
    StructuredBuffer<DrawData> draws = ResourceDescriptorHeap[Root.drawBufferIndex];
    return draws[Root.drawIndex];
}

MaterialGpuData LoadMaterial(uint materialIndex)
{
    StructuredBuffer<MaterialGpuData> materials = ResourceDescriptorHeap[Root.materialBufferIndex];
    return materials[materialIndex];
}

float4 SampleTexture(uint textureIndex, float2 uv)
{
    Texture2D<float4> texture = ResourceDescriptorHeap[textureIndex];
    return texture.Sample(AnisotropicWrap, uv);
}

struct VertexAttributes
{
    float3 normal;
    float4 tangent;
    float2 uv;
};

VertexAttributes LoadAttributes(uint vertexId)
{
    ByteAddressBuffer attributes = ResourceDescriptorHeap[Root.attributeBufferIndex];
    uint offset = vertexId * AttributeStride;

    VertexAttributes result;
    result.normal = asfloat(attributes.Load3(offset));
    result.tangent = asfloat(attributes.Load4(offset + 12));
    result.uv = asfloat(attributes.Load2(offset + 28));
    return result;
}

// Every pass must produce bit-identical depth for the opaque EQUAL test.
float4 TransformToClip(uint vertexId, float4x4 world)
{
    ByteAddressBuffer positions = ResourceDescriptorHeap[Root.positionBufferIndex];
    float3 localPosition = asfloat(positions.Load3(vertexId * PositionStride));

    precise float4 worldPosition = mul(float4(localPosition, 1.0), world);
    precise float4 clipPosition = mul(worldPosition, View.viewProjection);
    return clipPosition;
}

// --- Depth prepass -----------------------------------------------------------------------

float4 VSDepth(uint vertexId : SV_VertexID) : SV_Position
{
    return TransformToClip(vertexId, LoadDrawData().world);
}

struct MaskedDepthVertex
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    nointerpolation uint materialIndex : MATERIAL;
};

MaskedDepthVertex VSDepthMasked(uint vertexId : SV_VertexID)
{
    DrawData draw = LoadDrawData();

    MaskedDepthVertex output;
    output.position = TransformToClip(vertexId, draw.world);
    output.uv = LoadAttributes(vertexId).uv;
    output.materialIndex = draw.materialIndex;
    return output;
}

void PSDepthMasked(MaskedDepthVertex input)
{
    MaterialGpuData material = LoadMaterial(input.materialIndex);
    float alpha = material.baseColorFactor.a * SampleTexture(material.baseColorTexture, input.uv).a;
    clip(alpha - material.alphaCutoff);
}

// --- Opaque shading ----------------------------------------------------------------------

struct OpaqueVertex
{
    float4 position : SV_Position;
    float3 worldNormal : NORMAL;
    float4 worldTangent : TANGENT;
    float2 uv : TEXCOORD0;
    nointerpolation uint materialIndex : MATERIAL;
};

OpaqueVertex VSMain(uint vertexId : SV_VertexID)
{
    DrawData draw = LoadDrawData();
    VertexAttributes attributes = LoadAttributes(vertexId);

    OpaqueVertex output;
    output.position = TransformToClip(vertexId, draw.world);
    output.worldNormal = mul(float4(attributes.normal, 0.0), draw.normalMatrix).xyz;
    output.worldTangent = float4(mul(float4(attributes.tangent.xyz, 0.0), draw.world).xyz, attributes.tangent.w);
    output.uv = attributes.uv;
    output.materialIndex = draw.materialIndex;
    return output;
}

// Tangent-space normal from a BC5 (RG) map; Z is rebuilt from the unit length.
float3 GetShadingNormal(OpaqueVertex input, MaterialGpuData material, bool isFrontFace)
{
    float3 normal = normalize(input.worldNormal);
    float3 tangent = normalize(input.worldTangent.xyz - normal * dot(normal, input.worldTangent.xyz));
    float3 bitangent = cross(normal, tangent) * input.worldTangent.w;

    // Back faces of double-sided materials are lit from their own side.
    if (!isFrontFace)
    {
        normal = -normal;
        tangent = -tangent;
        bitangent = -bitangent;
    }

    float2 xy = SampleTexture(material.normalTexture, input.uv).xy * 2.0 - 1.0;
    float z = sqrt(saturate(1.0 - dot(xy, xy)));
    return normalize(tangent * xy.x + bitangent * xy.y + normal * z);
}

// Temporary lighting until the deferred PBR path (Phase 3): lambert sun + flat ambient.
// Metallic/roughness are already in the material data but not used yet.
float4 PSMain(OpaqueVertex input, bool isFrontFace : SV_IsFrontFace) : SV_Target
{
    MaterialGpuData material = LoadMaterial(input.materialIndex);

    float4 baseColor = material.baseColorFactor * SampleTexture(material.baseColorTexture, input.uv);
    float occlusion = SampleTexture(material.occlusionTexture, input.uv).r;
    float3 emissive = material.emissiveFactor * SampleTexture(material.emissiveTexture, input.uv).rgb;

    float3 normal = GetShadingNormal(input, material, isFrontFace);
    float sunAmount = saturate(dot(normal, Pass.sunDirection));

    float3 lighting = Pass.ambientColor * occlusion + Pass.sunColor * Pass.sunIntensity * sunAmount;
    return float4(baseColor.rgb * lighting + emissive, 1.0);
}
