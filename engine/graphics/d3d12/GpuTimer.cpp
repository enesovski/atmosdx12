#include "graphics/d3d12/GpuTimer.h"
#include "graphics/d3d12/GraphicsDevice.h"

namespace atmos
{
    namespace
    {
        constexpr uint32_t QueriesPerFrame = 2;
    }

    GpuTimer::GpuTimer(GraphicsDevice& device)
    {
        ID3D12Device* nativeDevice = device.GetNativeDevice();

        D3D12_QUERY_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        heapDesc.Count = QueriesPerFrame * GraphicsConfig::FramesInFlight;
        ThrowIfFailed(nativeDevice->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&queryHeap_)), "Failed to create query heap");
        SetDebugName(queryHeap_.Get(), L"GPU Timer Queries");

        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = heapDesc.Count * sizeof(uint64_t);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(nativeDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc,
                                                            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                            IID_PPV_ARGS(&readback_)),
                      "Failed to create timer readback buffer");
        SetDebugName(readback_.Get(), L"GPU Timer Readback");

        uint64_t frequency = 0;
        ThrowIfFailed(device.GetGraphicsQueue()->GetTimestampFrequency(&frequency), "Failed to query timestamp frequency");
        ticksToMilliseconds_ = 1000.0 / static_cast<double>(frequency);
    }

    void GpuTimer::Begin(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex)
    {
        // The slot's previous frame has completed, so its resolved timestamps are readable.
        if (slotHasResult_[frameIndex])
        {
            const D3D12_RANGE range{ frameIndex * QueriesPerFrame * sizeof(uint64_t),
                                     (frameIndex + 1) * QueriesPerFrame * sizeof(uint64_t) };
            void* mapped = nullptr;

            if (SUCCEEDED(readback_->Map(0, &range, &mapped)))
            {
                const uint64_t* ticks = static_cast<const uint64_t*>(mapped) + frameIndex * QueriesPerFrame;
                lastFrameMs_ = static_cast<float>(static_cast<double>(ticks[1] - ticks[0]) * ticksToMilliseconds_);
                const D3D12_RANGE noWrite{ 0, 0 };
                readback_->Unmap(0, &noWrite);
            }
        }

        commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, frameIndex * QueriesPerFrame);
    }

    void GpuTimer::End(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex)
    {
        const uint32_t first = frameIndex * QueriesPerFrame;
        commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first + 1);
        commandList->ResolveQueryData(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first, QueriesPerFrame, readback_.Get(),
                                      first * sizeof(uint64_t));
        slotHasResult_[frameIndex] = true;
    }
}
