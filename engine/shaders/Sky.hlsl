#include "Common.hlsli"

// Mirrors SkyConstants in SkyPass.cpp.
struct SkyConstants
{
    float3 sunDirection;
    float sunIntensity;
};

ConstantBuffer<SkyConstants> Constants : register(b0);

FullscreenVertex VSMain(uint vertexId : SV_VertexID)
{
    return MakeFullscreenVertex(vertexId);
}

float3 GetSkyColor(float3 viewDirection, float3 sunDirection)
{
    float sunHeight = sunDirection.y;
    float dayFactor = smoothstep(-0.18, 0.25, sunHeight);
    float twilight = smoothstep(-0.30, -0.02, sunHeight) *
                     (1.0 - smoothstep(0.08, 0.48, sunHeight));

    float3 nightZenith = float3(0.004, 0.009, 0.035);
    float3 nightHorizon = float3(0.025, 0.035, 0.080);
    float3 dayZenith = float3(0.035, 0.145, 0.455);
    float3 dayHorizon = float3(0.54, 0.73, 0.96);

    float3 zenith = lerp(nightZenith, dayZenith, dayFactor);
    float3 horizon = lerp(nightHorizon, dayHorizon, dayFactor);

    float2 viewHorizontal = viewDirection.xz / max(length(viewDirection.xz), 0.001);
    float2 sunHorizontal = sunDirection.xz / max(length(sunDirection.xz), 0.001);
    float towardSun = pow(saturate(dot(viewHorizontal, sunHorizontal)), 4.0);

    float3 warmHorizon = lerp(float3(1.0, 0.33, 0.16), float3(1.0, 0.53, 0.40),
                              smoothstep(-0.10, 0.20, sunHeight));

    horizon = lerp(horizon, warmHorizon, twilight * towardSun * 0.92);
    zenith = lerp(zenith, float3(0.19, 0.12, 0.34), twilight * 0.25);

    float gradient = smoothstep(-0.12, 0.88, viewDirection.y);
    return lerp(horizon, zenith, gradient);
}

float3 GetSunColor(float3 viewDirection, float3 sunDirection)
{
    float alignment = dot(viewDirection, sunDirection);
    float sunDisk = smoothstep(0.99945, 0.99975, alignment);
    float glow = pow(saturate(alignment), 96.0) * 0.38;
    float visibility = smoothstep(-0.08, 0.05, sunDirection.y);

    float3 warmSun = float3(1.0, 0.55, 0.30);
    float3 noonSun = float3(1.0, 0.96, 0.80);
    float3 color = lerp(warmSun, noonSun, smoothstep(0.03, 0.65, sunDirection.y));

    return color * (sunDisk + glow) * visibility * Constants.sunIntensity;
}

float4 PSMain(FullscreenVertex input) : SV_Target
{
    float3 viewDirection = GetViewDirection(input.uv);
    float3 sunDirection = Constants.sunDirection;

    float3 color = GetSkyColor(viewDirection, sunDirection);
    color += GetSunColor(viewDirection, sunDirection);

    return float4(color, 1.0);
}
