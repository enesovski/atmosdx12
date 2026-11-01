
static const float PI = 3.14159265359;

cbuffer SkyConstants : register(b0)
{
    float4 cameraRight;
    float4 cameraUp;
    float4 cameraForward;
    float4 params;
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

float3 GetViewDirection(float2 uv)
{
    float2 screen = uv * 2.0 - 1.0;
    screen.y = -screen.y;

    float aspectRatio = params.y;
    float tanHalfFovY = params.z;

    float3 direction =
        cameraForward.xyz +
        screen.x * aspectRatio * tanHalfFovY * cameraRight.xyz +
        screen.y * tanHalfFovY * cameraUp.xyz;

    return normalize(direction);
}

float3 GetSkyColor(float3 viewDirection, float sunHeight)
{
    float dayFactor = smoothstep(-0.15, 0.30, sunHeight);

    float3 nightZenith = float3(0.005, 0.010, 0.035);
    float3 nightHorizon = float3(0.055, 0.075, 0.14);

    float3 dayZenith = float3(0.025, 0.12, 0.40);
    float3 dayHorizon = float3(0.50, 0.73, 0.95);

    float3 zenith = lerp(nightZenith, dayZenith, dayFactor);
    float3 horizon = lerp(nightHorizon, dayHorizon, dayFactor);

    float sunsetFactor =
        (1.0 - smoothstep(0.0, 0.45, abs(sunHeight))) * dayFactor;

    float3 sunsetColor = float3(1.0, 0.36, 0.16);

    horizon = lerp(horizon, sunsetColor, sunsetFactor * 0.85);

    float gradient = smoothstep(-0.12, 0.85, viewDirection.y);

    return lerp(horizon, zenith, gradient);
}

float3 GetSunColor(float3 viewDirection, float3 sunDirection, float sunHeight)
{
    float alignment = dot(viewDirection, sunDirection);

    float sunDisk = smoothstep(0.9992, 0.9997, alignment);
    float glow = pow(saturate(alignment), 96.0) * 0.35;

    float visibility = smoothstep(-0.08, 0.10, sunHeight);

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

    float time = params.x;
    float dayDuration = 90.0;
    float angle = time * (2.0 * PI / dayDuration) + 0.2;

    // The sun moves in world space, independently of camera rotation.
    float3 sunDirection = normalize(float3(
        0.45 * cos(angle),
        sin(angle),
        0.8
    ));

    float sunHeight = sunDirection.y;
    float3 viewDirection = GetViewDirection(uv);

    float3 skyColor = GetSkyColor(viewDirection, sunHeight);
    float3 sunColor = GetSunColor(viewDirection, sunDirection, sunHeight);

    float3 finalColor = saturate(skyColor + sunColor);

    return float4(finalColor, 1.0);
}
