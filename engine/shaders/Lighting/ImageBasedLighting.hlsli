#ifndef ATMOS_IMAGE_BASED_LIGHTING_HLSLI
#define ATMOS_IMAGE_BASED_LIGHTING_HLSLI

#include "Lighting/BRDF.hlsli"
#include "Lighting/LightingCommon.hlsli"

// Sky lighting: diffuse from 3rd-order spherical harmonics (Ramamoorthi & Hanrahan 2001),
// specular from a GGX-prefiltered cube map plus the split-sum BRDF LUT (Karis 2013).

// The nine SH basis functions for a unit direction.
void EvaluateSHBasis(float3 n, out float basis[9])
{
    basis[0] = 0.282095;
    basis[1] = 0.488603 * n.y;
    basis[2] = 0.488603 * n.z;
    basis[3] = 0.488603 * n.x;
    basis[4] = 1.092548 * n.x * n.y;
    basis[5] = 1.092548 * n.y * n.z;
    basis[6] = 0.315392 * (3.0 * n.z * n.z - 1.0);
    basis[7] = 1.092548 * n.x * n.z;
    basis[8] = 0.546274 * (n.x * n.x - n.y * n.y);
}

// Coefficients are stored already convolved with the cosine lobe, so this is irradiance / PI.
float3 EvaluateIrradiance(float3 normal)
{
    StructuredBuffer<float4> coefficients = ResourceDescriptorHeap[Lighting.irradianceSHIndex];

    float basis[9];
    EvaluateSHBasis(normal, basis);

    float3 irradiance = 0.0;

    [unroll]
    for (uint i = 0; i < 9; ++i)
        irradiance += coefficients[i].rgb * basis[i];

    return max(irradiance, 0.0);
}

float3 EvaluateImageBasedLighting(Surface surface)
{
    if (!Lighting.imageBasedLightingEnabled)
        return 0.0;

    float NoV = saturate(dot(surface.normal, surface.view));

    Texture2D<float2> brdfLut = ResourceDescriptorHeap[Lighting.brdfLutIndex];
    float2 scaleBias = brdfLut.SampleLevel(LinearClamp, float2(NoV, surface.roughness), 0);

    float3 kS = F_SchlickRoughness(surface.f0, NoV, surface.roughness);
    float3 diffuse = surface.diffuseColor * EvaluateIrradiance(surface.normal) * (1.0 - kS);

    TextureCube<float4> specularCube = ResourceDescriptorHeap[Lighting.specularCubeIndex];
    float3 reflection = reflect(-surface.view, surface.normal);
    float mip = surface.roughness * (Lighting.specularMipCount - 1.0);
    float3 prefiltered = specularCube.SampleLevel(LinearClamp, reflection, mip).rgb;
    float3 specular = prefiltered * (surface.f0 * scaleBias.x + scaleBias.y);

    return (diffuse + specular) * surface.occlusion * Lighting.ambientIntensity;
}

#endif
