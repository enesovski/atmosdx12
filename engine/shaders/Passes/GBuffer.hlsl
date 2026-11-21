#include "Scene/GBuffer.hlsli"
#include "Scene/MaterialSurface.hlsli"

// Writes material properties for deferred lighting. Depth comes from the prepass (EQUAL test),
// so masked materials need no alpha test here.

MeshVertex VSMain(uint vertexId : SV_VertexID)
{
    return ProcessMeshVertex(vertexId);
}

GBufferOutput PSMain(MeshVertex input, bool isFrontFace : SV_IsFrontFace)
{
    MaterialSurface material = EvaluateMaterial(input, isFrontFace);

    GBufferData data;
    data.baseColor = material.baseColor;
    data.occlusion = material.occlusion;
    data.normal = material.normal;
    data.metallic = material.metallic;
    data.roughness = material.roughness;
    data.shadingModel = ShadingModelDefaultLit;

    return EncodeGBuffer(data, material.emissive);
}
