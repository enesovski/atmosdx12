#include "graphics/d3d12/UploadContext.h"
#include "graphics/d3d12/Buffer.h"
#include "graphics/d3d12/GraphicsDevice.h"
#include "core/Assert.h"

#include <cstring>

namespace atmos
{
    UploadContext::UploadContext(GraphicsDevice& device)
        : device_(device.GetNativeDevice()), queue_(device.GetGraphicsQueue())
    {
        ThrowIfFailed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_)),
                      "Failed to create upload allocator");
        SetDebugName(allocator_.Get(), L"Upload Allocator");

        ThrowIfFailed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr,
                                                 IID_PPV_ARGS(&commandList_)),
                      "Failed to create upload command list");
        SetDebugName(commandList_.Get(), L"Upload Command List");
        ThrowIfFailed(commandList_->Close(), "Failed to close upload command list");

        ThrowIfFailed(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)),
                      "Failed to create upload fence");
        SetDebugName(fence_.Get(), L"Upload Fence");

        fenceEvent_.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));

        if (!fenceEvent_)
            throw std::runtime_error("Failed to create upload fence event");
    }

    void UploadContext::UploadBuffer(const Buffer& destination, const void* data, uint64_t size)
    {
        ATMOS_ASSERT(size <= destination.GetSize(), "Upload larger than destination buffer");
        BeginRecording();

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

        void* mapped = nullptr;
        const D3D12_RANGE noRead{ 0, 0 };
        ThrowIfFailed(staging->Map(0, &noRead, &mapped), "Failed to map staging buffer");
        std::memcpy(mapped, data, size);
        staging->Unmap(0, nullptr);

        // The destination is in COMMON and is implicitly promoted to COPY_DEST.
        commandList_->CopyBufferRegion(destination.GetResource(), 0, staging.Get(), 0, size);
        stagingBuffers_.push_back(std::move(staging));
    }

    void UploadContext::Flush()
    {
        if (!recording_)
            return;

        ThrowIfFailed(commandList_->Close(), "Failed to close upload command list");

        ID3D12CommandList* lists[] = { commandList_.Get() };
        queue_->ExecuteCommandLists(1, lists);

        ThrowIfFailed(queue_->Signal(fence_.Get(), ++fenceValue_), "Failed to signal upload fence");
        ThrowIfFailed(fence_->SetEventOnCompletion(fenceValue_, fenceEvent_.get()), "Failed to wait for uploads");
        WaitForSingleObject(fenceEvent_.get(), INFINITE);

        stagingBuffers_.clear();
        recording_ = false;
    }

    void UploadContext::BeginRecording()
    {
        if (recording_)
            return;

        ThrowIfFailed(allocator_->Reset(), "Failed to reset upload allocator");
        ThrowIfFailed(commandList_->Reset(allocator_.Get(), nullptr), "Failed to reset upload command list");
        recording_ = true;
    }
}
