#include "graphics/d3d12/UploadQueue.h"
#include "graphics/d3d12/Buffer.h"
#include "graphics/d3d12/GraphicsDevice.h"
#include "graphics/d3d12/Texture.h"
#include "core/Assert.h"

#include <cstring>

namespace atmos
{
    UploadQueue::UploadQueue(GraphicsDevice& device)
        : device_(device.GetNativeDevice()), queue_(device.GetCopyQueue())
    {
        ThrowIfFailed(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)),
                      "Failed to create upload fence");
        SetDebugName(fence_.Get(), L"Upload Fence");

        fenceEvent_.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));

        if (!fenceEvent_)
            throw std::runtime_error("Failed to create upload fence event");
    }

    UploadQueue::~UploadQueue()
    {
        try
        {
            Submit();
            WaitIdle();
        }
        catch (const std::exception&)
        {
            // Device loss is reported by the renderer; nothing useful to add here.
        }
    }

    void UploadQueue::UploadBuffer(const Buffer& destination, const void* data, uint64_t size)
    {
        ATMOS_ASSERT(size <= destination.GetSize(), "Upload larger than destination buffer");
        EnsureRecording();

        void* mapped = nullptr;
        ComPtr<ID3D12Resource> staging = CreateStagingBuffer(size, &mapped);
        std::memcpy(mapped, data, size);
        staging->Unmap(0, nullptr);

        commandList_->CopyBufferRegion(destination.GetResource(), 0, staging.Get(), 0, size);
        recording_.stagingBuffers.push_back(std::move(staging));
    }

    void UploadQueue::UploadTexture(const Texture& destination, std::span<const TextureSubresourceData> subresources)
    {
        const D3D12_RESOURCE_DESC desc = destination.GetResource()->GetDesc();
        const UINT count = static_cast<UINT>(subresources.size());
        ATMOS_ASSERT(count == static_cast<UINT>(desc.MipLevels) * desc.DepthOrArraySize, "Every subresource needs data");
        EnsureRecording();

        // The driver decides the staging layout (row pitch alignment); rows are copied into it.
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(count);
        std::vector<UINT> rowCounts(count);
        std::vector<UINT64> rowSizes(count);
        UINT64 totalSize = 0;
        device_->GetCopyableFootprints(&desc, 0, count, 0, layouts.data(), rowCounts.data(), rowSizes.data(),
                                       &totalSize);

        void* mapped = nullptr;
        ComPtr<ID3D12Resource> staging = CreateStagingBuffer(totalSize, &mapped);

        for (UINT i = 0; i < count; ++i)
        {
            auto* dst = static_cast<std::byte*>(mapped) + layouts[i].Offset;
            auto* src = static_cast<const std::byte*>(subresources[i].data);

            for (UINT row = 0; row < rowCounts[i]; ++row)
            {
                std::memcpy(dst + row * layouts[i].Footprint.RowPitch, src + row * subresources[i].rowPitch,
                            rowSizes[i]);
            }

            D3D12_TEXTURE_COPY_LOCATION target{};
            target.pResource = destination.GetResource();
            target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            target.SubresourceIndex = i;

            D3D12_TEXTURE_COPY_LOCATION source{};
            source.pResource = staging.Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source.PlacedFootprint = layouts[i];

            commandList_->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);
        }

        staging->Unmap(0, nullptr);
        recording_.stagingBuffers.push_back(std::move(staging));
    }

    uint64_t UploadQueue::Submit()
    {
        if (!isRecording_)
            return 0;

        ThrowIfFailed(commandList_->Close(), "Failed to close upload command list");

        ID3D12CommandList* lists[] = { commandList_.Get() };
        queue_->ExecuteCommandLists(1, lists);

        recording_.fenceValue = nextFenceValue_++;
        ThrowIfFailed(queue_->Signal(fence_.Get(), recording_.fenceValue), "Failed to signal upload fence");

        const uint64_t fenceValue = recording_.fenceValue;
        inFlight_.push_back(std::move(recording_));
        recording_ = {};
        isRecording_ = false;
        return fenceValue;
    }

    bool UploadQueue::IsComplete(uint64_t fenceValue) const
    {
        return fence_->GetCompletedValue() >= fenceValue;
    }

    void UploadQueue::ReleaseCompleted()
    {
        const uint64_t completed = fence_->GetCompletedValue();

        while (!inFlight_.empty() && inFlight_.front().fenceValue <= completed)
        {
            freeAllocators_.push_back(std::move(inFlight_.front().allocator));
            inFlight_.pop_front();
        }
    }

    void UploadQueue::WaitIdle()
    {
        const uint64_t lastValue = nextFenceValue_ - 1;

        if (fence_->GetCompletedValue() < lastValue)
        {
            ThrowIfFailed(fence_->SetEventOnCompletion(lastValue, fenceEvent_.get()), "Failed to wait for uploads");
            WaitForSingleObject(fenceEvent_.get(), INFINITE);
        }

        ReleaseCompleted();
    }

    void UploadQueue::EnsureRecording()
    {
        if (isRecording_)
            return;

        if (!freeAllocators_.empty())
        {
            recording_.allocator = std::move(freeAllocators_.back());
            freeAllocators_.pop_back();
            ThrowIfFailed(recording_.allocator->Reset(), "Failed to reset upload allocator");
        }
        else
        {
            ThrowIfFailed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,
                                                          IID_PPV_ARGS(&recording_.allocator)),
                          "Failed to create upload allocator");
            SetDebugName(recording_.allocator.Get(), L"Upload Allocator");
        }

        if (!commandList_)
        {
            ThrowIfFailed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, recording_.allocator.Get(),
                                                     nullptr, IID_PPV_ARGS(&commandList_)),
                          "Failed to create upload command list");
            SetDebugName(commandList_.Get(), L"Upload Command List");
        }
        else
        {
            ThrowIfFailed(commandList_->Reset(recording_.allocator.Get(), nullptr),
                          "Failed to reset upload command list");
        }

        isRecording_ = true;
    }

    ComPtr<ID3D12Resource> UploadQueue::CreateStagingBuffer(uint64_t size, void** mappedData)
    {
        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ComPtr<ID3D12Resource> staging;
        ThrowIfFailed(device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc,
                                                       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                       IID_PPV_ARGS(&staging)),
                      "Failed to create staging buffer");

        const D3D12_RANGE noRead{ 0, 0 };
        ThrowIfFailed(staging->Map(0, &noRead, mappedData), "Failed to map staging buffer");
        return staging;
    }
}
