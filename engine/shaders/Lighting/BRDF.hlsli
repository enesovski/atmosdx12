#ifndef ATMOS_BRDF_HLSLI
#define ATMOS_BRDF_HLSLI

#include "Common.hlsli"

// Cook-Torrance microfacet specular with GGX (Walter 2007), height-correlated Smith visibility
// (Heitz 2014) and Schlick Fresnel, plus Lambert diffuse; the model used by Unreal and Filament.
// `roughness` is perceptual; alpha = roughness^2.

static const float MinRoughness = 0.045;  // keeps highlights finite for half-float storage

float D_GGX(float NoH, float alpha)
{
    float alpha2 = alpha * alpha;
    float f = (NoH * alpha2 - NoH) * NoH + 1.0;
    return alpha2 / (PI * f * f);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float alpha)
{
    float alpha2 = alpha * alpha;
    float lambdaV = NoL * sqrt(NoV * NoV * (1.0 - alpha2) + alpha2);
    float lambdaL = NoV * sqrt(NoL * NoL * (1.0 - alpha2) + alpha2);
    return 0.5 / max(lambdaV + lambdaL, 1e-5);
}

float3 F_Schlick(float3 f0, float VoH)
{
    float f = pow(1.0 - VoH, 5.0);
    return f0 + (1.0 - f0) * f;
}

float3 F_SchlickRoughness(float3 f0, float NoV, float roughness)
{
    return f0 + (max(1.0 - roughness, f0) - f0) * pow(1.0 - NoV, 5.0);
}

float Fd_Lambert()
{
    return 1.0 / PI;
}

#endif
