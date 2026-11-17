#pragma once

#include "graphics/d3d12/D3D12Utils.h"

#include <windows.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace atmos
{
    class Buffer;
    class GraphicsDevice;

    // Load-time uploads into GPU-only resources. Data is copied to staging memory right away;
    // Flush() submits all recorded copies and blocks until they finish.
    // Uses its own allocator, command list and fence, so it never touches frame state.
    class UploadContext
    {
    public:
        explicit UploadContext(GraphicsDevice& device);

        UploadContext(const UploadContext&) = delete;
        UploadContext& operator=(const UploadContext&) = delete;

        void UploadBuffer(const Buffer& destination, const void* data, uint64_t size);
        void Flush();

    private:
        void BeginRecording();

        ID3D12Device* device_;
        ID3D12CommandQueue* queue_;
        ComPtr<ID3D12CommandAllocator> allocator_;
        ComPtr<ID3D12GraphicsCommandList> commandList_;
        ComPtr<ID3D12Fence> fence_;
        std::unique_ptr<void, decltype(&CloseHandle)> fenceEvent_{ nullptr, &CloseHandle };
        uint64_t fenceValue_ = 0;
        bool recording_ = false;
        std::vector<ComPtr<ID3D12Resource>> stagingBuffers_;
    };
}
