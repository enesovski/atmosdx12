#include "graphics/d3d12/CommandContext.h"
#include "graphics/d3d12/Buffer.h"
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

    void CommandContext::Transition(Buffer& buffer, D3D12_RESOURCE_STATES newState)
    {
        if (buffer.state_ == newState)
            return;

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = buffer.GetResource();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = buffer.state_;
        barrier.Transition.StateAfter = newState;

        pendingBarriers_.push_back(barrier);
        buffer.state_ = newState;
    }

    void CommandContext::MarkDecayedToCommon(Buffer& buffer)
    {
        if (buffer.GetDesc().memory == BufferMemory::GpuOnly)
            buffer.state_ = D3D12_RESOURCE_STATE_COMMON;
    }

    void CommandContext::UavBarrier(ID3D12Resource* resource)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = resource;
        pendingBarriers_.push_back(barrier);
    }

    void CommandContext::FlushBarriers()
    {
        if (pendingBarriers_.empty())
            return;

        commandList_->ResourceBarrier(static_cast<UINT>(pendingBarriers_.size()), pendingBarriers_.data());
        pendingBarriers_.clear();
    }

    void CommandContext::SetRenderTargets(std::span<Texture* const> colors, Texture* depth, bool readOnlyDepth,
                                          uint32_t depthSlice)
    {
        ATMOS_ASSERT(!colors.empty() || depth, "No render targets given");
        ATMOS_ASSERT(colors.size() <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT, "Too many render targets");

        D3D12_CPU_DESCRIPTOR_HANDLE rtvs[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT]{};

        for (size_t i = 0; i < colors.size(); ++i)
        {
            Transition(*colors[i], D3D12_RESOURCE_STATE_RENDER_TARGET);
            rtvs[i] = colors[i]->GetRtv();
        }

        D3D12_CPU_DESCRIPTOR_HANDLE dsv{};

        if (depth)
        {
            if (!readOnlyDepth)
                Transition(*depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            else if (!(depth->GetState() & D3D12_RESOURCE_STATE_DEPTH_READ))  // Keep e.g. DEPTH_READ | SRV.
                Transition(*depth, D3D12_RESOURCE_STATE_DEPTH_READ);

            dsv = depth->GetDsv(depthSlice, readOnlyDepth);
        }

        commandList_->OMSetRenderTargets(static_cast<UINT>(colors.size()), rtvs, FALSE, depth ? &dsv : nullptr);

        const Texture& sizeSource = colors.empty() ? *depth : *colors[0];
        SetViewport(0, 0, sizeSource.GetWidth(), sizeSource.GetHeight());
    }

    void CommandContext::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        const D3D12_VIEWPORT viewport{ static_cast<float>(x), static_cast<float>(y), static_cast<float>(width),
                                       static_cast<float>(height), 0.0f, 1.0f };
        const D3D12_RECT scissor{ static_cast<LONG>(x), static_cast<LONG>(y), static_cast<LONG>(x + width),
                                  static_cast<LONG>(y + height) };

        commandList_->RSSetViewports(1, &viewport);
        commandList_->RSSetScissorRects(1, &scissor);
    }

    void CommandContext::SetRenderTarget(Texture& color, Texture* depth)
    {
        Texture* colors[] = { &color };
        SetRenderTargets(colors, depth);
    }

    void CommandContext::SetIndexBuffer(const Buffer& indices)
    {
        const D3D12_INDEX_BUFFER_VIEW view{ indices.GetGpuAddress(), static_cast<UINT>(indices.GetSize()),
                                            DXGI_FORMAT_R32_UINT };
        commandList_->IASetIndexBuffer(&view);
    }

    void CommandContext::DrawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t instanceCount)
    {
        ATMOS_ASSERT(boundPipelineType_ == PipelineType::Graphics, "Draw needs a graphics pipeline");
        FlushBarriers();
        commandList_->DrawIndexedInstanced(indexCount, instanceCount, startIndex, 0, 0);
    }

    void CommandContext::ClearRenderTarget(Texture& color)
    {
        Transition(color, D3D12_RESOURCE_STATE_RENDER_TARGET);
        FlushBarriers();
        commandList_->ClearRenderTargetView(color.GetRtv(), color.GetDesc().clearColor.data(), 0, nullptr);
    }

    void CommandContext::ClearDepth(Texture& depth, uint32_t slice)
    {
        Transition(depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        FlushBarriers();
        commandList_->ClearDepthStencilView(depth.GetDsv(slice), D3D12_CLEAR_FLAG_DEPTH, depth.GetDesc().clearDepth, 0, 0,
                                            nullptr);
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
