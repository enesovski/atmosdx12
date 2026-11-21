#ifndef ATMOS_SKY_MODEL_HLSLI
#define ATMOS_SKY_MODEL_HLSLI

// Stylized analytic sky driven by sun height; replaced by the physical atmosphere in Phase 4.
// Shared by the sky pass and the sky-light capture, so lighting always matches what is visible.

float3 GetSkyRadiance(float3 viewDirection, float3 sunDirection)
{
    float sunHeight = sunDirection.y;
    float dayFactor = smoothstep(-0.18, 0.25, sunHeight);
    float twilight = smoothstep(-0.30, -0.02, sunHeight) * (1.0 - smoothstep(0.08, 0.48, sunHeight));

    float3 nightZenith = float3(0.004, 0.009, 0.035);
    float3 nightHorizon = float3(0.025, 0.035, 0.080);
    float3 dayZenith = float3(0.035, 0.145, 0.455);
    float3 dayHorizon = float3(0.54, 0.73, 0.96);

    float3 zenith = lerp(nightZenith, dayZenith, dayFactor);
    float3 horizon = lerp(nightHorizon, dayHorizon, dayFactor);

    float2 viewHorizontal = viewDirection.xz / max(length(viewDirection.xz), 0.001);
    float2 sunHorizontal = sunDirection.xz / max(length(sunDirection.xz), 0.001);
    float towardSun = pow(saturate(dot(viewHorizontal, sunHorizontal)), 4.0);

    float3 warmHorizon = lerp(float3(1.0, 0.33, 0.16), float3(1.0, 0.53, 0.40), smoothstep(-0.10, 0.20, sunHeight));

    horizon = lerp(horizon, warmHorizon, twilight * towardSun * 0.92);
    zenith = lerp(zenith, float3(0.19, 0.12, 0.34), twilight * 0.25);

    // Below the horizon, fade toward a dim ground color instead of mirroring the sky.
    float gradient = smoothstep(-0.12, 0.88, viewDirection.y);
    float3 sky = lerp(horizon, zenith, gradient);
    float3 ground = horizon * 0.25;
    return lerp(ground, sky, smoothstep(-0.2, 0.0, viewDirection.y));
}

float3 GetSunDiskRadiance(float3 viewDirection, float3 sunDirection, float intensity)
{
    float alignment = dot(viewDirection, sunDirection);
    float disk = smoothstep(0.99945, 0.99975, alignment);
    float glow = pow(saturate(alignment), 96.0) * 0.38;
    float visibility = smoothstep(-0.08, 0.05, sunDirection.y);

    float3 warmSun = float3(1.0, 0.55, 0.30);
    float3 noonSun = float3(1.0, 0.96, 0.80);
    float3 color = lerp(warmSun, noonSun, smoothstep(0.03, 0.65, sunDirection.y));

    return color * (disk + glow) * visibility * intensity;
}

#endif
