
#include "GraphicsPipeline.h"

#include <d3dcompiler.h>

#include <climits>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace
{
    void Check(HRESULT result, const char* message)
    {
        if (FAILED(result))
            throw std::runtime_error(message);
    }

    ComPtr<ID3DBlob> CompileShader(
        const std::filesystem::path& path,
        const std::string& entryPoint,
        const char* target
    )
    {
        ComPtr<ID3DBlob> shader;
        ComPtr<ID3DBlob> errors;

        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        HRESULT result = D3DCompileFromFile(
            path.c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint.c_str(),
            target,
            flags,
            0,
            &shader,
            &errors
        );

        if (FAILED(result))
        {
            std::string message = "Failed to compile shader: ";

            if (errors)
            {
                message.append(
                    static_cast<const char*>(errors->GetBufferPointer()),
                    errors->GetBufferSize()
                );
            }
            else
            {
                message += path.string();
            }

            throw std::runtime_error(message);
        }

        return shader;
    }
}

GraphicsPipeline::GraphicsPipeline(
    ID3D12Device* device,
    const std::filesystem::path& shaderPath,
    const std::string& vertexEntry,
    const std::string& pixelEntry,
    DXGI_FORMAT renderTargetFormat
)
{
    ComPtr<ID3DBlob> vertexShader =
        CompileShader(shaderPath, vertexEntry, "vs_5_1");

    ComPtr<ID3DBlob> pixelShader =
        CompileShader(shaderPath, pixelEntry, "ps_5_1");

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};

    rootDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> errors;

    HRESULT result = D3D12SerializeRootSignature(
        &rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &errors
    );

    if (FAILED(result))
    {
        std::string message = "Failed to serialize root signature";

        if (errors)
        {
            message += ": ";
            message.append(
                static_cast<const char*>(errors->GetBufferPointer()),
                errors->GetBufferSize()
            );
        }

        throw std::runtime_error(message);
    }

    Check(device->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_)
    ), "Failed to create root signature");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();

    desc.VS = {
        vertexShader->GetBufferPointer(),
        vertexShader->GetBufferSize()
    };

    desc.PS = {
        pixelShader->GetBufferPointer(),
        pixelShader->GetBufferSize()
    };

    desc.BlendState.AlphaToCoverageEnable = FALSE;
    desc.BlendState.IndependentBlendEnable = FALSE;

    D3D12_RENDER_TARGET_BLEND_DESC blendDesc{};
    blendDesc.BlendEnable = FALSE;
    blendDesc.LogicOpEnable = FALSE;
    blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    desc.BlendState.RenderTarget[0] = blendDesc;

    desc.SampleMask = UINT_MAX;

    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.FrontCounterClockwise = FALSE;
    desc.RasterizerState.DepthClipEnable = TRUE;

    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.StencilEnable = FALSE;

    desc.InputLayout = { nullptr, 0 };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = renderTargetFormat;
    desc.SampleDesc.Count = 1;

    Check(device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(&pipelineState_)
    ), "Failed to create graphics pipeline state");
}

void GraphicsPipeline::Bind(
    ID3D12GraphicsCommandList* commandList
) const
{
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
}
