#include "Lighting/BRDF.hlsli"
#include "Lighting/ImageBasedLighting.hlsli"
#include "Lighting/Sampling.hlsli"
#include "Sky/SkyModel.hlsli"

// Sky-light capture and filtering (graphics/lighting/SkyLight.cpp), one entry point per step:
//   CSCaptureSky       sky radiance into a cube map (no sun disk; the sun is a direct light)
//   CSProjectIrradiance  cube map -> 9 SH coefficients, pre-convolved with the cosine lobe
//   CSPrefilterSpecular  GGX-prefiltered cube mip for one roughness
//   CSIntegrateBrdf      split-sum BRDF LUT (scale, bias), computed once

// Mirrors SkyLightConstants in SkyLight.cpp.
struct SkyLightConstants
{
    float3 sunDirection;
    uint sourceIndex;
    uint outputIndex;
    uint outputSize;
    float roughness;
    uint padding;
};

ConstantBuffer<SkyLightConstants> Constants : register(b0);

[numthreads(8, 8, 1)]
void CSCaptureSky(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= Constants.outputSize))
        return;

    float3 direction = GetCubeDirection(id.z, (id.xy + 0.5) / Constants.outputSize);
    RWTexture2DArray<float4> output = ResourceDescriptorHeap[Constants.outputIndex];
    output[id] = float4(GetSkyRadiance(direction, Constants.sunDirection), 1.0);
}

static const uint ProjectThreads = 256;
groupshared float3 SharedCoefficients[ProjectThreads][9];

// Sums radiance * basis * solid angle over every texel of the source cube, then convolves with
// the cosine lobe (A0 = PI, A1 = 2PI/3, A2 = PI/4) and divides by PI for Lambert.
[numthreads(ProjectThreads, 1, 1)]
void CSProjectIrradiance(uint threadIndex : SV_GroupIndex)
{
    TextureCube<float4> source = ResourceDescriptorHeap[Constants.sourceIndex];
    uint size = Constants.outputSize;
    uint texelCount = size * size * 6;

    float3 sum[9];
    [unroll] for (uint c = 0; c < 9; ++c) sum[c] = 0.0;

    for (uint texel = threadIndex; texel < texelCount; texel += ProjectThreads)
    {
        uint face = texel / (size * size);
        uint2 xy = uint2(texel % size, (texel / size) % size);
        float2 st = ((xy + 0.5) / size) * 2.0 - 1.0;
        float solidAngle = 4.0 / (pow(1.0 + dot(st, st), 1.5) * size * size);

        float3 direction = GetCubeDirection(face, (xy + 0.5) / size);
        float3 radiance = source.SampleLevel(PointClamp, direction, 0).rgb;

        float basis[9];
        EvaluateSHBasis(direction, basis);

        [unroll] for (uint c = 0; c < 9; ++c) sum[c] += radiance * basis[c] * solidAngle;
    }

    [unroll] for (uint c = 0; c < 9; ++c) SharedCoefficients[threadIndex][c] = sum[c];
    GroupMemoryBarrierWithGroupSync();

    for (uint stride = ProjectThreads / 2; stride > 0; stride /= 2)
    {
        if (threadIndex < stride)
        {
            [unroll] for (uint c = 0; c < 9; ++c)
                SharedCoefficients[threadIndex][c] += SharedCoefficients[threadIndex + stride][c];
        }

        GroupMemoryBarrierWithGroupSync();
    }

    if (threadIndex < 9)
    {
        static const float bandScale[9] = { 1.0, 2.0 / 3.0, 2.0 / 3.0, 2.0 / 3.0, 0.25, 0.25, 0.25, 0.25, 0.25 };
        RWStructuredBuffer<float4> output = ResourceDescriptorHeap[Constants.outputIndex];
        output[threadIndex] = float4(SharedCoefficients[0][threadIndex] * bandScale[threadIndex], 0.0);
    }
}

[numthreads(8, 8, 1)]
void CSPrefilterSpecular(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= Constants.outputSize))
        return;

    TextureCube<float4> source = ResourceDescriptorHeap[Constants.sourceIndex];
    float3 normal = GetCubeDirection(id.z, (id.xy + 0.5) / Constants.outputSize);

    static const uint SampleCount = 64;
    float3 color = 0.0;
    float weight = 0.0;

    // Split-sum assumption: view = normal = reflection direction.
    for (uint i = 0; i < SampleCount; ++i)
    {
        float3 halfVector = ImportanceSampleGGX(Hammersley(i, SampleCount), normal, Constants.roughness);
        float3 lightDirection = 2.0 * dot(normal, halfVector) * halfVector - normal;
        float NoL = saturate(dot(normal, lightDirection));

        if (NoL > 0.0)
        {
            color += source.SampleLevel(LinearClamp, lightDirection, 0).rgb * NoL;
            weight += NoL;
        }
    }

    RWTexture2DArray<float4> output = ResourceDescriptorHeap[Constants.outputIndex];
    output[id] = float4(color / max(weight, 1e-4), 1.0);
}

[numthreads(8, 8, 1)]
void CSIntegrateBrdf(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= Constants.outputSize))
        return;

    float NoV = max((id.x + 0.5) / Constants.outputSize, 1e-3);
    float roughness = max((id.y + 0.5) / Constants.outputSize, MinRoughness);
    float alpha = roughness * roughness;

    float3 view = float3(sqrt(1.0 - NoV * NoV), 0.0, NoV);
    float3 normal = float3(0.0, 0.0, 1.0);

    static const uint SampleCount = 256;
    float scale = 0.0;
    float bias = 0.0;

    for (uint i = 0; i < SampleCount; ++i)
    {
        float3 halfVector = ImportanceSampleGGX(Hammersley(i, SampleCount), normal, roughness);
        float3 lightDirection = 2.0 * dot(view, halfVector) * halfVector - view;
        float NoL = saturate(lightDirection.z);
        float NoH = saturate(halfVector.z);
        float VoH = saturate(dot(view, halfVector));

        if (NoL > 0.0)
        {
            float visibility = V_SmithGGXCorrelated(NoV, NoL, alpha) * 4.0 * NoL * VoH / max(NoH, 1e-4);
            float fresnel = pow(1.0 - VoH, 5.0);
            scale += (1.0 - fresnel) * visibility;
            bias += fresnel * visibility;
        }
    }

    RWTexture2D<float2> output = ResourceDescriptorHeap[Constants.outputIndex];
    output[id.xy] = float2(scale, bias) / SampleCount;
}
