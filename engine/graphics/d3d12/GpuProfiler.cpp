#include "graphics/d3d12/GpuProfiler.h"
#include "graphics/d3d12/GraphicsDevice.h"

namespace atmos
{
    GpuProfiler::GpuProfiler(GraphicsDevice& device)
    {
        ID3D12Device* nativeDevice = device.GetNativeDevice();

        D3D12_QUERY_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        heapDesc.Count = QueriesPerFrame * GraphicsConfig::FramesInFlight;
        ThrowIfFailed(nativeDevice->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&queryHeap_)), "Failed to create query heap");
        SetDebugName(queryHeap_.Get(), L"GPU Profiler Queries");

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
                      "Failed to create profiler readback buffer");
        SetDebugName(readback_.Get(), L"GPU Profiler Readback");

        uint64_t frequency = 0;
        ThrowIfFailed(device.GetGraphicsQueue()->GetTimestampFrequency(&frequency), "Failed to query timestamp frequency");
        ticksToMilliseconds_ = 1000.0 / static_cast<double>(frequency);
    }

    void GpuProfiler::BeginFrame(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex)
    {
        frameIndex_ = frameIndex;

        // The slot's previous frame has completed, so its resolved timestamps are readable.
        if (slots_[frameIndex].hasResult)
            ReadBack(frameIndex);

        slots_[frameIndex].scopeNames.clear();
        slots_[frameIndex].hasResult = false;
        commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, GetQueryIndex(0));
    }

    void GpuProfiler::EndFrame(ID3D12GraphicsCommandList* commandList)
    {
        FrameSlot& slot = slots_[frameIndex_];
        const uint32_t first = GetQueryIndex(0);
        const uint32_t count = 2 + 2 * static_cast<uint32_t>(slot.scopeNames.size());

        commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, GetQueryIndex(1));
        commandList->ResolveQueryData(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first, count, readback_.Get(),
                                      first * sizeof(uint64_t));
        slot.hasResult = true;
    }

    uint32_t GpuProfiler::BeginScope(ID3D12GraphicsCommandList* commandList, const std::wstring& name)
    {
        FrameSlot& slot = slots_[frameIndex_];

        if (slot.scopeNames.size() >= MaxScopesPerFrame)
            return NoScope;

        const auto scope = static_cast<uint32_t>(slot.scopeNames.size());
        slot.scopeNames.push_back(name);
        commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, GetQueryIndex(2 + 2 * scope));
        return scope;
    }

    void GpuProfiler::EndScope(ID3D12GraphicsCommandList* commandList, uint32_t scope)
    {
        if (scope != NoScope)
            commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, GetQueryIndex(3 + 2 * scope));
    }

    void GpuProfiler::ReadBack(uint32_t frameIndex)
    {
        const FrameSlot& slot = slots_[frameIndex];
        const size_t first = static_cast<size_t>(frameIndex) * QueriesPerFrame;
        const size_t count = 2 + 2 * slot.scopeNames.size();
        const D3D12_RANGE range{ first * sizeof(uint64_t), (first + count) * sizeof(uint64_t) };
        void* mapped = nullptr;

        if (FAILED(readback_->Map(0, &range, &mapped)))
            return;

        const uint64_t* ticks = static_cast<const uint64_t*>(mapped) + first;
        auto toMilliseconds = [&](uint64_t begin, uint64_t end) {
            return end > begin ? static_cast<float>(static_cast<double>(end - begin) * ticksToMilliseconds_) : 0.0f;
        };

        lastFrameMs_ = toMilliseconds(ticks[0], ticks[1]);
        lastScopes_.resize(slot.scopeNames.size());

        for (size_t i = 0; i < slot.scopeNames.size(); ++i)
            lastScopes_[i] = { slot.scopeNames[i], toMilliseconds(ticks[2 + 2 * i], ticks[3 + 2 * i]) };

        const D3D12_RANGE noWrite{ 0, 0 };
        readback_->Unmap(0, &noWrite);
    }
}
