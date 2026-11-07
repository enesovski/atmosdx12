
#pragma once

#include <d3d12.h>
#include <wrl/client.h>

class GraphicsPipeline
{
public:
    // Uses its owned root signature; all other pipeline state is supplied by the caller.
    GraphicsPipeline(ID3D12Device* device, const D3D12_ROOT_SIGNATURE_DESC& rootDesc,
                     D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc);

    void Bind(ID3D12GraphicsCommandList* commandList) const;

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
