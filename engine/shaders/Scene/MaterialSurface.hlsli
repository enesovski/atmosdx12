#ifndef ATMOS_MATERIAL_SURFACE_HLSLI
#define ATMOS_MATERIAL_SURFACE_HLSLI

#include "Scene/SceneData.hlsli"

// The interpolated mesh vertex shared by the GBuffer and transparent passes.
struct MeshVertex
{
    float4 position : SV_Position;
    float3 worldPosition : POSITION;
    float3 worldNormal : NORMAL;
    float4 worldTangent : TANGENT;
    float2 uv : TEXCOORD0;
    nointerpolation uint materialIndex : MATERIAL;
};

MeshVertex ProcessMeshVertex(uint vertexId)
{
    DrawData draw = LoadDrawData();
    VertexAttributes attributes = LoadAttributes(vertexId);
    float3 localPosition = LoadPosition(vertexId);

    MeshVertex output;
    output.position = TransformToClip(localPosition, draw.world);
    output.worldPosition = mul(float4(localPosition, 1.0), draw.world).xyz;
    output.worldNormal = mul(float4(attributes.normal, 0.0), draw.normalMatrix).xyz;
    output.worldTangent = float4(mul(float4(attributes.tangent.xyz, 0.0), draw.world).xyz, attributes.tangent.w);
    output.uv = attributes.uv;
    output.materialIndex = draw.materialIndex;
    return output;
}

// Everything a material contributes at one pixel (glTF metallic-roughness).
struct MaterialSurface
{
    float3 baseColor;
    float alpha;
    float3 normal;
    float metallic;
    float roughness;
    float occlusion;
    float3 emissive;
};

// Tangent-space normal from a BC5 (RG) map; Z is rebuilt from unit length.
float3 GetShadingNormal(MeshVertex input, MaterialData material, bool isFrontFace)
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

    float2 xy = SampleMaterialTexture(material.normalTexture, input.uv).xy * 2.0 - 1.0;
    float z = sqrt(saturate(1.0 - dot(xy, xy)));
    return normalize(tangent * xy.x + bitangent * xy.y + normal * z);
}

MaterialSurface EvaluateMaterial(MeshVertex input, bool isFrontFace)
{
    MaterialData material = LoadMaterial(input.materialIndex);

    float4 baseColor = material.baseColorFactor * SampleMaterialTexture(material.baseColorTexture, input.uv);
    float4 metallicRoughness = SampleMaterialTexture(material.metallicRoughnessTexture, input.uv);

    MaterialSurface surface;
    surface.baseColor = baseColor.rgb;
    surface.alpha = baseColor.a;
    surface.normal = GetShadingNormal(input, material, isFrontFace);
    surface.metallic = saturate(material.metallicFactor * metallicRoughness.b);
    surface.roughness = saturate(material.roughnessFactor * metallicRoughness.g);
    surface.occlusion = SampleMaterialTexture(material.occlusionTexture, input.uv).r;
    surface.emissive = material.emissiveFactor * SampleMaterialTexture(material.emissiveTexture, input.uv).rgb;
    return surface;
}

#endif
