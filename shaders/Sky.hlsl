
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

float4 PSMain(VertexOutput input) : SV_TARGET
{
    float3 zenith = float3(0.025, 0.06, 0.20);
    float3 horizon = float3(0.55, 0.75, 0.95);

    float t = saturate(input.uv.y);
    float3 color = lerp(zenith, horizon, t);

    return float4(color, 1.0);
}
