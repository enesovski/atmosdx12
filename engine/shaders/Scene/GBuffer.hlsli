#ifndef ATMOS_GBUFFER_HLSLI
#define ATMOS_GBUFFER_HLSLI

// GBuffer layout (formats in graphics/passes/GBufferPass.h):
//   A  R8G8B8A8_UNORM_SRGB   base color.rgb, material occlusion
//   B  R10G10B10A2_UNORM     octahedral normal.xy, unused, shading model
//   C  R8G8B8A8_UNORM        metallic, roughness, unused, unused
//   Scene color receives emissive directly; lighting adds on top.

static const uint ShadingModelUnlit = 0;
static const uint ShadingModelDefaultLit = 1;

struct GBufferData
{
    float3 baseColor;
    float occlusion;
    float3 normal;
    float metallic;
    float roughness;
    uint shadingModel;
};

struct GBufferOutput
{
    float4 a : SV_Target0;
    float4 b : SV_Target1;
    float4 c : SV_Target2;
    float4 emissive : SV_Target3;
};

// Octahedral normal encoding (Cigolle et al. 2014), [-1, 1] -> [0, 1].
float2 EncodeOctahedral(float3 n)
{
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    float2 wrapped = (1.0 - abs(n.yx)) * select(n.xy >= 0.0, 1.0, -1.0);
    return (n.z >= 0.0 ? n.xy : wrapped) * 0.5 + 0.5;
}

float3 DecodeOctahedral(float2 encoded)
{
    float2 f = encoded * 2.0 - 1.0;
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += select(n.xy >= 0.0, -t, t);
    return normalize(n);
}

GBufferOutput EncodeGBuffer(GBufferData data, float3 emissive)
{
    GBufferOutput output;
    output.a = float4(data.baseColor, data.occlusion);
    output.b = float4(EncodeOctahedral(data.normal), 0.0, data.shadingModel / 3.0);
    output.c = float4(data.metallic, data.roughness, 0.0, 0.0);
    output.emissive = float4(emissive, 1.0);
    return output;
}

GBufferData DecodeGBuffer(float4 a, float4 b, float4 c)
{
    GBufferData data;
    data.baseColor = a.rgb;
    data.occlusion = a.a;
    data.normal = DecodeOctahedral(b.xy);
    data.shadingModel = (uint)round(b.a * 3.0);
    data.metallic = c.r;
    data.roughness = c.g;
    return data;
}

#endif
