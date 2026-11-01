
static const float PI = 3.14159265359;

cbuffer SkyConstants : register(b0)
{
    float time;
    float2 resolution;
    float padding;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(uint vertexID : SV_VertexID)
{
    VertexOutput output;

    float2 uv = float2(
        (vertexID << 1) & 2,
        vertexID & 2
    );

    output.position = float4(
        uv.x * 2.0 - 1.0,
        1.0 - uv.y * 2.0,
        0.0,
        1.0
    );

    output.uv = uv;

    return output;
}

float3 GetSkyColor(float2 uv, float sunHeight)
{
    float dayFactor = smoothstep(-0.15, 0.30, sunHeight);

    float3 nightZenith = float3(0.005, 0.010, 0.035);
    float3 nightHorizon = float3(0.055, 0.075, 0.14);

    float3 dayZenith = float3(0.025, 0.12, 0.40);
    float3 dayHorizon = float3(0.50, 0.73, 0.95);

    float3 zenith = lerp(
        nightZenith,
        dayZenith,
        dayFactor
    );

    float3 horizon = lerp(
        nightHorizon,
        dayHorizon,
        dayFactor
    );

    float sunsetFactor =
        (1.0 - smoothstep(0.0, 0.45, abs(sunHeight))) *
        dayFactor;

    float3 sunsetColor = float3(1.0, 0.36, 0.16);

    horizon = lerp(
        horizon,
        sunsetColor,
        sunsetFactor * 0.85
    );

    float gradient = smoothstep(0.0, 0.85, uv.y);

    return lerp(zenith, horizon, gradient);
}

float3 GetSunColor(float2 uv, float2 sunPosition, float sunHeight)
{
    float2 delta = uv - sunPosition;

    delta.x *= resolution.x / max(resolution.y, 1.0);

    float distanceToSun = length(delta);

    float sunDisk = 1.0 - smoothstep(
        0.025,
        0.030,
        distanceToSun
    );

    float glow = exp(-distanceToSun * 12.0) * 0.35;

    float visibility = smoothstep(
        -0.05,
        0.08,
        sunHeight
    );

    float3 sunColor = lerp(
        float3(1.0, 0.45, 0.15),
        float3(1.0, 0.95, 0.75),
        smoothstep(0.0, 0.50, sunHeight)
    );

    return sunColor * (sunDisk + glow) * visibility;
}

float4 PSMain(VertexOutput input) : SV_TARGET
{
    float2 uv = saturate(input.uv);

    // One complete day takes 90 seconds.
    float dayDuration = 90.0;
    float angle = time * (2.0 * PI / dayDuration) + 0.5;

    float sunHeight = sin(angle);

    float2 sunPosition = float2(
        0.5 + 0.30 * cos(angle),
        0.70 - 0.42 * sin(angle)
    );

    float3 skyColor = GetSkyColor(uv, sunHeight);
    float3 sunColor = GetSunColor(uv, sunPosition, sunHeight);

    float3 finalColor = saturate(skyColor + sunColor);

    return float4(finalColor, 1.0);
}
