#include "graphics/lighting/SkyLight.h"
#include "graphics/RenderScene.h"
#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/ShaderCompiler.h"

#include <cmath>
#include <format>

namespace atmos
{
    namespace
    {
        constexpr uint32_t GroupSize = 8;
        constexpr D3D12_RESOURCE_STATES ShaderReadState =
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        // Mirrors SkyLightConstants in Passes/SkyLight.hlsl.
        struct SkyLightConstants
        {
            DirectX::XMFLOAT3 sunDirection;
            uint32_t sourceIndex;
            uint32_t outputIndex;
            uint32_t outputSize;
            float roughness;
            uint32_t padding;
        };

        PipelineState CreatePipeline(const GraphicsDevice& device, const ShaderCompiler& compiler, const wchar_t* entry)
        {
            return PipelineState::CreateCompute(device, {
                .debugName = std::wstring(L"Sky Light ") + (entry + 2),
                .computeShader = compiler.Compile(L"Passes/SkyLight.hlsl", entry, ShaderStage::Compute).Get(),
            });
        }

        Texture CreateCube(GraphicsDevice& device, uint32_t size, uint32_t mips, const wchar_t* name)
        {
            return Texture(device, {
                .width = size,
                .height = size,
                .mipLevels = mips,
                .arraySize = 6,
                .cube = true,
                .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
                .usage = TextureUsage::ShaderResource | TextureUsage::UnorderedAccess,
                .debugName = name,
            });
        }

        uint32_t GroupCount(uint32_t size)
        {
            return (size + GroupSize - 1) / GroupSize;
        }

        bool NearlyEqual(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
        {
            return std::abs(a.x - b.x) < 1e-4f && std::abs(a.y - b.y) < 1e-4f && std::abs(a.z - b.z) < 1e-4f;
        }
    }

    SkyLight::SkyLight(GraphicsDevice& device, const ShaderCompiler& compiler)
        : skyCube_(CreateCube(device, CaptureSize, 1, L"Sky Light Capture")),
          specularCube_(CreateCube(device, SpecularSize, SpecularMipCount, L"Sky Light Specular")),
          brdfLut_(device, {
              .width = BrdfLutSize,
              .height = BrdfLutSize,
              .format = DXGI_FORMAT_R16G16_FLOAT,
              .usage = TextureUsage::ShaderResource | TextureUsage::UnorderedAccess,
              .debugName = L"BRDF LUT",
          }),
          irradianceSH_(device, {
              .size = 9 * sizeof(DirectX::XMFLOAT4),
              .stride = sizeof(DirectX::XMFLOAT4),
              .usage = BufferUsage::ShaderResource | BufferUsage::UnorderedAccess,
              .debugName = L"Sky Light Irradiance SH",
          }),
          capturePipeline_(CreatePipeline(device, compiler, L"CSCaptureSky")),
          projectPipeline_(CreatePipeline(device, compiler, L"CSProjectIrradiance")),
          prefilterPipeline_(CreatePipeline(device, compiler, L"CSPrefilterSpecular")),
          brdfPipeline_(CreatePipeline(device, compiler, L"CSIntegrateBrdf"))
    {
    }

    SkyLightResources SkyLight::AddToGraph(RenderGraph& graph, const SkyEnvironment& sky)
    {
        const RGTexture skyCube = graph.ImportTexture(skyCube_, ShaderReadState);
        const SkyLightResources resources{
            .irradianceSH = graph.ImportBuffer(irradianceSH_),
            .specularCube = graph.ImportTexture(specularCube_, ShaderReadState),
            .brdfLut = graph.ImportTexture(brdfLut_, ShaderReadState),
            .specularMipCount = SpecularMipCount,
        };

        if (!brdfLutReady_)
        {
            graph.AddPass(L"Sky Light BRDF LUT",
                [&](RGBuilder& builder) { builder.ReadWriteTexture(resources.brdfLut); },
                [this, lut = resources.brdfLut](RGContext& context) {
                    context.commands.SetPipeline(brdfPipeline_);
                    context.commands.SetRootConstants(
                        SkyLightConstants{ {}, 0, context.GetTexture(lut).GetUavIndex(), BrdfLutSize, 0.0f, 0 });
                    context.commands.Dispatch(GroupCount(BrdfLutSize), GroupCount(BrdfLutSize));
                });

            brdfLutReady_ = true;
        }

        const DirectX::XMFLOAT3 sunDirection = sky.GetSunDirection();

        if (capturedSunDirection_ && NearlyEqual(*capturedSunDirection_, sunDirection))
            return resources;

        capturedSunDirection_ = sunDirection;

        graph.AddPass(L"Sky Light Capture",
            [&](RGBuilder& builder) { builder.ReadWriteTexture(skyCube); },
            [this, skyCube, sunDirection](RGContext& context) {
                context.commands.SetPipeline(capturePipeline_);
                context.commands.SetRootConstants(
                    SkyLightConstants{ sunDirection, 0, context.GetTexture(skyCube).GetUavIndex(), CaptureSize, 0.0f, 0 });
                context.commands.Dispatch(GroupCount(CaptureSize), GroupCount(CaptureSize), 6);
            });

        graph.AddPass(L"Sky Light Irradiance",
            [&](RGBuilder& builder) {
                builder.ReadTexture(skyCube);
                builder.ReadWriteBuffer(resources.irradianceSH);
            },
            [this, skyCube, sh = resources.irradianceSH](RGContext& context) {
                context.commands.SetPipeline(projectPipeline_);
                context.commands.SetRootConstants(SkyLightConstants{
                    {}, context.GetTexture(skyCube).GetSrvIndex(), context.GetBuffer(sh).GetUavIndex(), CaptureSize, 0.0f, 0 });
                context.commands.Dispatch(1);
            });

        for (uint32_t mip = 0; mip < SpecularMipCount; ++mip)
        {
            const uint32_t size = SpecularSize >> mip;
            const float roughness = static_cast<float>(mip) / (SpecularMipCount - 1);

            graph.AddPass(std::format(L"Sky Light Prefilter Mip {}", mip),
                [&](RGBuilder& builder) {
                    builder.ReadTexture(skyCube);
                    builder.ReadWriteTexture(resources.specularCube, mip);
                },
                [this, skyCube, specular = resources.specularCube, mip, size, roughness](RGContext& context) {
                    context.commands.SetPipeline(prefilterPipeline_);
                    context.commands.SetRootConstants(SkyLightConstants{ {}, context.GetTexture(skyCube).GetSrvIndex(),
                                                                         context.GetTexture(specular).GetUavIndex(mip),
                                                                         size, roughness, 0 });
                    context.commands.Dispatch(GroupCount(size), GroupCount(size), 6);
                });
        }

        return resources;
    }
}
