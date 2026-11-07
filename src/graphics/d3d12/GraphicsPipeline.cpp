
#include "GraphicsPipeline.h"
#include "DxError.h"

#include <d3dcompiler.h>

#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;

GraphicsPipeline::GraphicsPipeline(ID3D12Device* device,
                                   const D3D12_ROOT_SIGNATURE_DESC& rootDesc,
                                   D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc)
{
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
            message += "\n";
            message.append(
                static_cast<const char*>(errors->GetBufferPointer()),
                errors->GetBufferSize()
            );
        }

        throw std::runtime_error(message);
    }

    CheckHRESULT(device->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_)
    ), "Failed to create root signature");

    pipelineDesc.pRootSignature = rootSignature_.Get();

    CheckHRESULT(device->CreateGraphicsPipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(&pipelineState_)
    ), "Failed to create graphics pipeline state");
}

void GraphicsPipeline::Bind(ID3D12GraphicsCommandList* commandList) const
{
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
}
