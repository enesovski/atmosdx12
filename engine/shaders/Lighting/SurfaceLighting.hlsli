#ifndef ATMOS_SURFACE_LIGHTING_HLSLI
#define ATMOS_SURFACE_LIGHTING_HLSLI

#include "Lighting/BRDF.hlsli"
#include "Lighting/ImageBasedLighting.hlsli"
#include "Lighting/LightClusters.hlsli"
#include "Lighting/Shadows.hlsli"

// Shared by deferred lighting and forward transparents: sun + clustered local lights + sky.

// BRDF times NoL for one light direction.
float3 EvaluateBRDF(Surface surface, float3 lightDirection)
{
    float3 halfVector = normalize(surface.view + lightDirection);
    float NoV = abs(dot(surface.normal, surface.view)) + 1e-5;
    float NoL = saturate(dot(surface.normal, lightDirection));
    float NoH = saturate(dot(surface.normal, halfVector));
    float VoH = saturate(dot(surface.view, halfVector));

    float alpha = surface.roughness * surface.roughness;
    float3 F = F_Schlick(surface.f0, VoH);
    float3 specular = D_GGX(NoH, alpha) * V_SmithGGXCorrelated(NoV, NoL, alpha) * F;
    float3 diffuse = surface.diffuseColor * Fd_Lambert() * (1.0 - F);

    return (diffuse + specular) * NoL;
}

// Inverse-square falloff windowed to zero at the light's range (Karis 2013).
float GetDistanceAttenuation(float distanceSquared, float range)
{
    float ratio = distanceSquared / (range * range);
    float window = saturate(1.0 - ratio * ratio);
    return window * window / max(distanceSquared, 1e-4);
}

float GetSpotAttenuation(LightData light, float3 lightDirection)
{
    float cosAngle = dot(-lightDirection, light.direction);
    float attenuation = saturate(cosAngle * light.spotScale + light.spotOffset);
    return attenuation * attenuation;
}

float3 EvaluateLocalLight(Surface surface, LightData light)
{
    float3 toLight = light.position - surface.position;
    float distanceSquared = dot(toLight, toLight);

    if (distanceSquared > light.range * light.range)
        return 0.0;

    float3 lightDirection = toLight * rsqrt(distanceSquared);
    float attenuation = GetDistanceAttenuation(distanceSquared, light.range);

    if (light.type == LightTypeSpot)
        attenuation *= GetSpotAttenuation(light, lightDirection);

    if (attenuation <= 0.0)
        return 0.0;

    float shadow = SampleLocalShadow(light, surface.position, surface.normal);
    return EvaluateBRDF(surface, lightDirection) * light.color * attenuation * shadow;
}

float3 EvaluateSun(Surface surface, float viewDepth)
{
    float shadow = SampleSunShadow(surface.position, surface.normal, viewDepth);
    return EvaluateBRDF(surface, Lighting.sunDirection) * Lighting.sunColor * Lighting.sunIntensity * shadow;
}

float3 EvaluateLighting(Surface surface, float2 pixel, float viewDepth)
{
    float3 color = EvaluateSun(surface, viewDepth);

    uint clusterIndex = GetClusterIndex(GetClusterCoordinate(pixel, viewDepth));
    uint lightCount = GetClusterLightCount(clusterIndex);

    for (uint i = 0; i < lightCount; ++i)
        color += EvaluateLocalLight(surface, LoadLight(GetClusterLightIndex(clusterIndex, i)));

    return color + EvaluateImageBasedLighting(surface);
}

#endif
