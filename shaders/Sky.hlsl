static const float PI = 3.14159265359;

cbuffer SkyConstants : register(b0)
{
    float4 cameraRight;
    float4 cameraUp;
    float4 cameraForward;
    float4 viewParams;   // elapsed seconds, aspect, tanHalfFovY, time of day
    float4 sunParams;    // world direction xyz, intensity
    float4 moonParams;   // world direction xyz, intensity
    float4 nightParams;  // star intensity, unused
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(uint vertexID : SV_VertexID)
{
    VertexOutput output;

    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    output.uv = uv;

    return output;
}

float3 GetViewDirection(float2 uv)
{
    float2 screen = uv * 2.0 - 1.0;
    screen.y = -screen.y;

    float3 direction =
        cameraForward.xyz +
        screen.x * viewParams.y * viewParams.z * cameraRight.xyz +
        screen.y * viewParams.z * cameraUp.xyz;

    return normalize(direction);
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

    float evening = smoothstep(11.0, 13.0, viewParams.w);
    float3 sunrise = float3(1.0, 0.53, 0.40);
    float3 sunset = float3(1.0, 0.33, 0.16);
    float3 warmHorizon = lerp(sunrise, sunset, evening);

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

    return color * (sunDisk + glow) * visibility * sunParams.w;
}

float3 GetMoonColor(float3 viewDirection, float3 moonDirection, float sunHeight)
{
    float alignment = dot(viewDirection, moonDirection);
    float moonDisk = smoothstep(0.99920, 0.99960, alignment);
    float glow = pow(saturate(alignment), 190.0) * 0.12;

    float visibility = smoothstep(-0.03, 0.12, moonDirection.y) *
                       (1.0 - smoothstep(-0.15, 0.10, sunHeight));

    return float3(0.72, 0.83, 1.0) * (moonDisk + glow) * visibility * moonParams.w;
}

float Hash21(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float3 GetStars(float3 viewDirection, float sunHeight)
{
    if (sunHeight >= 0.02 || nightParams.x <= 0.0)
        return float3(0.0, 0.0, 0.0);

    float nightVisibility = 1.0 - smoothstep(-0.24, 0.02, sunHeight);
    float aboveHorizon = smoothstep(-0.05, 0.14, viewDirection.y);

    float longitude = atan2(viewDirection.z, viewDirection.x) / (2.0 * PI) + 0.5;
    float latitude = asin(clamp(viewDirection.y, -1.0, 1.0)) / PI + 0.5;

    float2 grid = float2(longitude * 220.0, latitude * 130.0);
    float2 cell = floor(grid);
    float2 localPosition = frac(grid);

    float seed = Hash21(cell);
    float present = step(0.955, seed);
    float2 starPosition = 0.15 + 0.70 * float2(
        Hash21(cell + float2(17.0, 37.0)),
        Hash21(cell + float2(53.0, 91.0))
    );

    float radius = lerp(0.035, 0.075, Hash21(cell + float2(11.0, 61.0)));
    float footprint = max(length(ddx(grid)), length(ddy(grid)));
    float distanceToStar = length(localPosition - starPosition);
    float star = 1.0 - smoothstep(radius, radius + footprint, distanceToStar);

    float twinkle = 0.84 + 0.16 * sin(viewParams.x * (0.9 + seed) + seed * 49.0);
    float colorSeed = Hash21(cell + float2(29.0, 73.0));
    float3 color = lerp(float3(0.65, 0.78, 1.0), float3(1.0, 0.95, 0.83), colorSeed);

    return color * star * present * twinkle * nightVisibility * aboveHorizon * nightParams.x;
}

float4 PSMain(VertexOutput input) : SV_TARGET
{
    float3 viewDirection = GetViewDirection(saturate(input.uv));
    float3 sunDirection = sunParams.xyz;
    float3 moonDirection = moonParams.xyz;

    float3 color = GetSkyColor(viewDirection, sunDirection);
    color += GetStars(viewDirection, sunDirection.y);
    color += GetSunColor(viewDirection, sunDirection);
    color += GetMoonColor(viewDirection, moonDirection, sunDirection.y);

    return float4(saturate(color), 1.0);
}