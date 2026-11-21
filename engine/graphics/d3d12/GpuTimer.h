#pragma once

#include "graphics/d3d12/D3D12Utils.h"
#include "graphics/d3d12/GraphicsConfig.h"

#include <cstdint>

namespace atmos
{
    class GraphicsDevice;

    // Measures whole-frame GPU time with timestamp queries. Results are read back when the frame
    // slot comes around again, so they lag FramesInFlight frames behind.
    class GpuTimer
    {
    public:
        explicit GpuTimer(GraphicsDevice& device);

        // Call right after FrameScheduler::BeginFrame waited on the slot.
        void Begin(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex);
        void End(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex);

        float GetLastFrameMilliseconds() const { return lastFrameMs_; }

    private:
        ComPtr<ID3D12QueryHeap> queryHeap_;
        ComPtr<ID3D12Resource> readback_;
        double ticksToMilliseconds_ = 0.0;
        float lastFrameMs_ = 0.0f;
        bool slotHasResult_[GraphicsConfig::FramesInFlight]{};
    };
}
