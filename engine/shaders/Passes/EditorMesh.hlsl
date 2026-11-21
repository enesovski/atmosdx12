#include "Scene/SceneData.hlsli"

// Editor mesh overlays: the wireframe over the final image and the selection mask that
// SelectionOutline.hlsl turns into an outline.

float4 VSMesh(uint vertexId : SV_VertexID) : SV_Position
{
    return TransformToClip(LoadPosition(vertexId), LoadDrawData().world);
}

struct MaskedVertex
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    nointerpolation uint materialIndex : MATERIAL;
};

MaskedVertex VSMeshMasked(uint vertexId : SV_VertexID)
{
    DrawData draw = LoadDrawData();

    MaskedVertex output;
    output.position = TransformToClip(LoadPosition(vertexId), draw.world);
    output.uv = LoadAttributes(vertexId).uv;
    output.materialIndex = draw.materialIndex;
    return output;
}

// Display-referred: the target already holds the tonemapped, sRGB-encoded image.
float4 PSWireframe() : SV_Target
{
    return float4(0.82, 0.86, 0.92, 0.35);
}

float PSMask() : SV_Target
{
    return 1.0;
}

float PSMaskMasked(MaskedVertex input) : SV_Target
{
    MaterialData material = LoadMaterial(input.materialIndex);
    float alpha = material.baseColorFactor.a * SampleMaterialTexture(material.baseColorTexture, input.uv).a;
    clip(alpha - material.alphaCutoff);
    return 1.0;
}
