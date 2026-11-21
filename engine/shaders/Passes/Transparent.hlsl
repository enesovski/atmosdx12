#include "Lighting/SurfaceLighting.hlsli"
#include "Scene/MaterialSurface.hlsli"

// Forward shading for alpha-blended materials, with the same lighting as the deferred path.

MeshVertex VSMain(uint vertexId : SV_VertexID)
{
    return ProcessMeshVertex(vertexId);
}

float4 PSMain(MeshVertex input, bool isFrontFace : SV_IsFrontFace) : SV_Target
{
    MaterialSurface material = EvaluateMaterial(input, isFrontFace);
    Surface surface = MakeSurface(input.worldPosition, material.normal, material.baseColor, material.metallic,
                                  material.roughness, material.occlusion);

    float viewDepth = GetViewDepth(input.worldPosition);
    float3 color = EvaluateLighting(surface, input.position.xy, viewDepth) + material.emissive;
    return float4(color, material.alpha);
}
