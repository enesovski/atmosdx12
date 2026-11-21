#pragma once

#include "graphics/d3d12/D3D12Utils.h"
#include "graphics/d3d12/GraphicsConfig.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace atmos
{
    class GraphicsDevice;

    struct GpuScopeTiming
    {
        std::wstring name;
        float milliseconds = 0.0f;
    };

    // GPU timestamps for the whole frame and for named scopes inside it (one per render-graph pass).
    // Results are read back when the frame slot comes around again, so they lag FramesInFlight frames.
    class GpuProfiler
    {
    public:
        static constexpr uint32_t MaxScopesPerFrame = 256;

        explicit GpuProfiler(GraphicsDevice& device);

        // Call right after FrameScheduler::BeginFrame waited on the slot.
        void BeginFrame(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex);
        void EndFrame(ID3D12GraphicsCommandList* commandList);

        // Returns a scope id for EndScope; scopes past MaxScopesPerFrame are ignored.
        uint32_t BeginScope(ID3D12GraphicsCommandList* commandList, const std::wstring& name);
        void EndScope(ID3D12GraphicsCommandList* commandList, uint32_t scope);

        float GetLastFrameMilliseconds() const { return lastFrameMs_; }
        const std::vector<GpuScopeTiming>& GetLastScopes() const { return lastScopes_; }

    private:
        static constexpr uint32_t NoScope = UINT32_MAX;
        static constexpr uint32_t QueriesPerFrame = 2 + 2 * MaxScopesPerFrame;  // frame begin/end, then scope pairs

        struct FrameSlot
        {
            std::vector<std::wstring> scopeNames;
            bool hasResult = false;
        };

        void ReadBack(uint32_t frameIndex);
        uint32_t GetQueryIndex(uint32_t query) const { return frameIndex_ * QueriesPerFrame + query; }

        ComPtr<ID3D12QueryHeap> queryHeap_;
        ComPtr<ID3D12Resource> readback_;
        double ticksToMilliseconds_ = 0.0;
        std::array<FrameSlot, GraphicsConfig::FramesInFlight> slots_;
        uint32_t frameIndex_ = 0;

        float lastFrameMs_ = 0.0f;
        std::vector<GpuScopeTiming> lastScopes_;
    };
}
