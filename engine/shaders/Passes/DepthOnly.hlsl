#include "Scene/SceneData.hlsli"

// Depth for any view: the camera prepass, shadow cascades and shadow-atlas tiles.
// Masked materials alpha-test against base color; opaque ones need no pixel shader.

float4 VSDepth(uint vertexId : SV_VertexID) : SV_Position
{
    return TransformToClip(LoadPosition(vertexId), LoadDrawData().world);
}

struct MaskedVertex
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    nointerpolation uint materialIndex : MATERIAL;
};

MaskedVertex VSDepthMasked(uint vertexId : SV_VertexID)
{
    DrawData draw = LoadDrawData();

    MaskedVertex output;
    output.position = TransformToClip(LoadPosition(vertexId), draw.world);
    output.uv = LoadAttributes(vertexId).uv;
    output.materialIndex = draw.materialIndex;
    return output;
}

void PSDepthMasked(MaskedVertex input)
{
    MaterialData material = LoadMaterial(input.materialIndex);
    float alpha = material.baseColorFactor.a * SampleMaterialTexture(material.baseColorTexture, input.uv).a;
    clip(alpha - material.alphaCutoff);
}
