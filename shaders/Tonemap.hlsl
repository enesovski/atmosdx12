
Texture2D hdrTexture : register(t0);
SamplerState linearSampler : register(s0);

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(uint vertexID : SV_VertexID)
{
    VertexOutput output;

    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);

    output.position = float4(
        uv.x * 2.0 - 1.0,
        1.0 - uv.y * 2.0,
        0.0,
        1.0
    );

    output.uv = uv;
    return output;
}

float3 LinearToSRGB(float3 color)
{
    float3 low = color * 12.92;
    float3 high = 1.055 * pow(max(color, 0.0), 1.0 / 2.4) - 0.055;

    return lerp(low, high, step(0.0031308, color));
}

float4 PSMain(VertexOutput input) : SV_TARGET
{
    float3 hdrColor = hdrTexture.Sample(
        linearSampler, saturate(input.uv)
    ).rgb;

    const float exposure = 1.0;

    float3 mapped = 1.0 - exp(-hdrColor * exposure);
    float3 srgb = LinearToSRGB(saturate(mapped));

    return float4(srgb, 1.0);
}
