#ifndef ATMOS_SAMPLING_HLSLI
#define ATMOS_SAMPLING_HLSLI

#include "Common.hlsli"

float2 Hammersley(uint i, uint count)
{
    return float2((float)i / count, reversebits(i) * 2.3283064365386963e-10);
}

// GGX-distributed half vector around `normal` (Karis 2013).
float3 ImportanceSampleGGX(float2 xi, float3 normal, float roughness)
{
    float alpha = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float3 h = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    float3 up = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangentX = normalize(cross(up, normal));
    float3 tangentY = cross(normal, tangentX);
    return tangentX * h.x + tangentY * h.y + normal * h.z;
}

// Direction through texel `uv` of cube face `face` (D3D order: +X, -X, +Y, -Y, +Z, -Z).
float3 GetCubeDirection(uint face, float2 uv)
{
    float2 st = uv * 2.0 - 1.0;
    float3 directions[6] = {
        float3(1.0, -st.y, -st.x),
        float3(-1.0, -st.y, st.x),
        float3(st.x, 1.0, st.y),
        float3(st.x, -1.0, -st.y),
        float3(st.x, -st.y, 1.0),
        float3(-st.x, -st.y, -1.0),
    };
    return normalize(directions[face]);
}

#endif
