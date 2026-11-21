#include "graphics/d3d12/BindlessRootSignature.h"

#include <array>
#include <string>

namespace atmos
{
    namespace
    {
        D3D12_ROOT_PARAMETER1 MakeRootCbv(uint32_t shaderRegister)
        {
            D3D12_ROOT_PARAMETER1 parameter{};
            parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            parameter.Descriptor.ShaderRegister = shaderRegister;
            parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            return parameter;
        }

        D3D12_STATIC_SAMPLER_DESC MakeStaticSampler(uint32_t shaderRegister, D3D12_FILTER filter,
                                                    D3D12_TEXTURE_ADDRESS_MODE addressMode)
        {
            D3D12_STATIC_SAMPLER_DESC sampler{};
            sampler.Filter = filter;
            sampler.AddressU = addressMode;
            sampler.AddressV = addressMode;
            sampler.AddressW = addressMode;
            sampler.MaxAnisotropy = 16;
            sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
            sampler.MaxLOD = D3D12_FLOAT32_MAX;
            sampler.ShaderRegister = shaderRegister;
            sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            return sampler;
        }
    }

    namespace
    {
        // Hardware 2x2 PCF; reverse-Z, so a sample is lit when its depth is at least the stored depth.
        D3D12_STATIC_SAMPLER_DESC MakeShadowSampler(uint32_t shaderRegister)
        {
            D3D12_STATIC_SAMPLER_DESC sampler = MakeStaticSampler(shaderRegister, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
                                                                  D3D12_TEXTURE_ADDRESS_MODE_BORDER);
            sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
            return sampler;
        }
    }

    ComPtr<ID3D12RootSignature> CreateBindlessRootSignature(ID3D12Device* device)
    {
        std::array<D3D12_ROOT_PARAMETER1, 3> parameters{};

        D3D12_ROOT_PARAMETER1& rootConstants = parameters[static_cast<size_t>(RootSlot::RootConstants)];
        rootConstants.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootConstants.Constants.ShaderRegister = 0;
        rootConstants.Constants.Num32BitValues = RootConstantCount;
        rootConstants.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        parameters[static_cast<size_t>(RootSlot::ViewConstants)] = MakeRootCbv(1);
        parameters[static_cast<size_t>(RootSlot::PassConstants)] = MakeRootCbv(2);

        // Registers s0-s4 match the sampler declarations in Common.hlsli.
        const std::array samplers = {
            MakeStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
            MakeStaticSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
            MakeStaticSampler(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
            MakeStaticSampler(3, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
            MakeShadowSampler(4),
        };

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
        desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        desc.Desc_1_1.NumParameters = static_cast<UINT>(parameters.size());
        desc.Desc_1_1.pParameters = parameters.data();
        desc.Desc_1_1.NumStaticSamplers = static_cast<UINT>(samplers.size());
        desc.Desc_1_1.pStaticSamplers = samplers.data();
        desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

        ComPtr<ID3DBlob> blob;
        ComPtr<ID3DBlob> error;

        if (FAILED(D3D12SerializeVersionedRootSignature(&desc, &blob, &error)))
        {
            std::string message = "Failed to serialize bindless root signature";

            if (error)
                message.append(": ").append(static_cast<const char*>(error->GetBufferPointer()),
                                            error->GetBufferSize());

            throw std::runtime_error(message);
        }

        ComPtr<ID3D12RootSignature> rootSignature;
        ThrowIfFailed(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                                  IID_PPV_ARGS(&rootSignature)),
                      "Failed to create bindless root signature");
        SetDebugName(rootSignature.Get(), L"Bindless Root Signature");

        return rootSignature;
    }
}
