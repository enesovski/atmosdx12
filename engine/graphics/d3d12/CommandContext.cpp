#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/GraphicsDevice.h"
#include "graphics/d3d12/Texture.h"
#include "graphics/d3d12/UploadAllocator.h"
#include "core/Assert.h"

#include <cstring>
#include <cwchar>

namespace atmos
{
    namespace
    {
        constexpr UINT ToIndex(RootSlot slot)
        {
            return static_cast<UINT>(slot);
        }
    }

    CommandContext::CommandContext(GraphicsDevice& device, UploadAllocator& uploadAllocator)
        : device_(device), uploadAllocator_(uploadAllocator)
    {
    }

    void CommandContext::Begin(ID3D12GraphicsCommandList* commandList)
    {
        ATMOS_ASSERT(pendingBarriers_.empty(), "Barriers left over from the previous frame");

        commandList_ = commandList;
        boundPipelineType_ = PipelineType::Graphics;

        // Directly indexed heaps must be set before the root signature.
        ID3D12DescriptorHeap* heaps[] = { device_.GetShaderResourceHeap().GetHeap() };
        commandList_->SetDescriptorHeaps(1, heaps);

        ID3D12RootSignature* rootSignature = device_.GetBindlessRootSignature();
        commandList_->SetGraphicsRootSignature(rootSignature);
        commandList_->SetComputeRootSignature(rootSignature);
    }

    void CommandContext::End()
    {
        FlushBarriers();
        commandList_ = nullptr;
    }

    void CommandContext::Transition(Texture& texture, D3D12_RESOURCE_STATES newState)
    {
        if (texture.state_ == newState)
            return;

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = texture.GetResource();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = texture.state_;
        barrier.Transition.StateAfter = newState;

        pendingBarriers_.push_back(barrier);
        texture.state_ = newState;
    }

    void CommandContext::FlushBarriers()
    {
        if (pendingBarriers_.empty())
            return;

        commandList_->ResourceBarrier(static_cast<UINT>(pendingBarriers_.size()), pendingBarriers_.data());
        pendingBarriers_.clear();
    }

    void CommandContext::SetRenderTarget(Texture& color, Texture* depth)
    {
        Transition(color, D3D12_RESOURCE_STATE_RENDER_TARGET);

        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = color.GetRtv();
        D3D12_CPU_DESCRIPTOR_HANDLE dsv{};

        if (depth)
        {
            Transition(*depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            dsv = depth->GetDsv();
        }

        commandList_->OMSetRenderTargets(1, &rtv, FALSE, depth ? &dsv : nullptr);

        const D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(color.GetWidth()),
                                       static_cast<float>(color.GetHeight()), 0.0f, 1.0f };
        const D3D12_RECT scissor{ 0, 0, static_cast<LONG>(color.GetWidth()),
                                  static_cast<LONG>(color.GetHeight()) };

        commandList_->RSSetViewports(1, &viewport);
        commandList_->RSSetScissorRects(1, &scissor);
    }

    void CommandContext::ClearRenderTarget(Texture& color)
    {
        Transition(color, D3D12_RESOURCE_STATE_RENDER_TARGET);
        FlushBarriers();
        commandList_->ClearRenderTargetView(color.GetRtv(), color.GetDesc().clearColor.data(), 0, nullptr);
    }

    void CommandContext::ClearDepth(Texture& depth)
    {
        Transition(depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        FlushBarriers();
        commandList_->ClearDepthStencilView(depth.GetDsv(), D3D12_CLEAR_FLAG_DEPTH,
                                            depth.GetDesc().clearDepth, 0, 0, nullptr);
    }

    void CommandContext::SetPipeline(const PipelineState& pipeline)
    {
        commandList_->SetPipelineState(pipeline.GetNative());
        boundPipelineType_ = pipeline.GetType();

        if (boundPipelineType_ == PipelineType::Graphics)
            commandList_->IASetPrimitiveTopology(pipeline.GetTopology());
    }

    void CommandContext::SetRootConstants(const void* data, uint32_t num32BitValues)
    {
        const UINT slot = ToIndex(RootSlot::RootConstants);

        if (boundPipelineType_ == PipelineType::Graphics)
            commandList_->SetGraphicsRoot32BitConstants(slot, num32BitValues, data, 0);
        else
            commandList_->SetComputeRoot32BitConstants(slot, num32BitValues, data, 0);
    }

    void CommandContext::SetConstantBuffer(RootSlot slot, D3D12_GPU_VIRTUAL_ADDRESS address)
    {
        ATMOS_ASSERT(slot != RootSlot::RootConstants, "Root constants are not a CBV slot");

        if (boundPipelineType_ == PipelineType::Graphics)
            commandList_->SetGraphicsRootConstantBufferView(ToIndex(slot), address);
        else
            commandList_->SetComputeRootConstantBufferView(ToIndex(slot), address);
    }

    void CommandContext::SetGlobalConstantBuffer(RootSlot slot, D3D12_GPU_VIRTUAL_ADDRESS address)
    {
        ATMOS_ASSERT(slot != RootSlot::RootConstants, "Root constants are not a CBV slot");

        commandList_->SetGraphicsRootConstantBufferView(ToIndex(slot), address);
        commandList_->SetComputeRootConstantBufferView(ToIndex(slot), address);
    }

    D3D12_GPU_VIRTUAL_ADDRESS CommandContext::UploadConstants(const void* data, size_t size)
    {
        const UploadAllocation allocation = uploadAllocator_.Allocate(size);
        std::memcpy(allocation.cpuAddress, data, size);
        return allocation.gpuAddress;
    }

    void CommandContext::Draw(uint32_t vertexCount, uint32_t instanceCount)
    {
        ATMOS_ASSERT(boundPipelineType_ == PipelineType::Graphics, "Draw needs a graphics pipeline");
        FlushBarriers();
        commandList_->DrawInstanced(vertexCount, instanceCount, 0, 0);
    }

    void CommandContext::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        ATMOS_ASSERT(boundPipelineType_ == PipelineType::Compute, "Dispatch needs a compute pipeline");
        FlushBarriers();
        commandList_->Dispatch(groupCountX, groupCountY, groupCountZ);
    }

    void CommandContext::BeginEvent(const wchar_t* name)
    {
        // Metadata 0 marks a plain UTF-16 string, understood by PIX and RenderDoc.
        const UINT size = static_cast<UINT>((std::wcslen(name) + 1) * sizeof(wchar_t));
        commandList_->BeginEvent(0, name, size);
    }

    void CommandContext::EndEvent()
    {
        commandList_->EndEvent();
    }
}
