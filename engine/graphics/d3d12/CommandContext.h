#pragma once

#include "graphics/d3d12/BindlessRootSignature.h"
#include "graphics/d3d12/D3D12Utils.h"
#include "graphics/d3d12/PipelineState.h"

#include <cstdint>
#include <type_traits>
#include <vector>

namespace atmos
{
    class GraphicsDevice;
    class Texture;
    class UploadAllocator;

    // Records one frame's GPU commands. Tracks texture states, batches barriers until the
    // next draw/dispatch/clear, and binds root arguments for whichever pipeline is bound.
    class CommandContext
    {
    public:
        CommandContext(GraphicsDevice& device, UploadAllocator& uploadAllocator);

        CommandContext(const CommandContext&) = delete;
        CommandContext& operator=(const CommandContext&) = delete;

        // Binds the descriptor heap and bindless root signature to an opened command list.
        void Begin(ID3D12GraphicsCommandList* commandList);
        void End();

        ID3D12GraphicsCommandList* GetNativeCommandList() const { return commandList_; }

        void Transition(Texture& texture, D3D12_RESOURCE_STATES newState);
        void FlushBarriers();

        // Transitions the targets and sets viewport/scissor to the color target's size.
        void SetRenderTarget(Texture& color, Texture* depth = nullptr);
        void ClearRenderTarget(Texture& color);
        void ClearDepth(Texture& depth);

        // Bind the pipeline before its root arguments; they follow its type.
        void SetPipeline(const PipelineState& pipeline);

        template <typename T>
        void SetRootConstants(const T& constants)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            static_assert(sizeof(T) % 4 == 0 && sizeof(T) <= RootConstantCount * 4,
                          "Root constants must be 4-byte multiples within the root budget");
            SetRootConstants(&constants, sizeof(T) / 4);
        }

        // Copies `data` into this frame's upload memory and binds it.
        template <typename T>
        void SetConstantBuffer(RootSlot slot, const T& data)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            SetConstantBuffer(slot, UploadConstants(&data, sizeof(T)));
        }

        // Binds for graphics and compute alike; for frame/view data shared by all passes.
        template <typename T>
        void SetGlobalConstantBuffer(RootSlot slot, const T& data)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            SetGlobalConstantBuffer(slot, UploadConstants(&data, sizeof(T)));
        }

        void SetConstantBuffer(RootSlot slot, D3D12_GPU_VIRTUAL_ADDRESS address);
        void SetGlobalConstantBuffer(RootSlot slot, D3D12_GPU_VIRTUAL_ADDRESS address);

        void Draw(uint32_t vertexCount, uint32_t instanceCount = 1);
        void DrawFullscreenTriangle() { Draw(3); }
        void Dispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1);

        // PIX/RenderDoc markers. Prefer ScopedGpuEvent.
        void BeginEvent(const wchar_t* name);
        void EndEvent();

    private:
        void SetRootConstants(const void* data, uint32_t num32BitValues);
        D3D12_GPU_VIRTUAL_ADDRESS UploadConstants(const void* data, size_t size);

        GraphicsDevice& device_;
        UploadAllocator& uploadAllocator_;
        ID3D12GraphicsCommandList* commandList_ = nullptr;
        PipelineType boundPipelineType_ = PipelineType::Graphics;
        std::vector<D3D12_RESOURCE_BARRIER> pendingBarriers_;
    };

    class ScopedGpuEvent
    {
    public:
        ScopedGpuEvent(CommandContext& context, const wchar_t* name) : context_(context)
        {
            context_.BeginEvent(name);
        }

        ~ScopedGpuEvent() { context_.EndEvent(); }

        ScopedGpuEvent(const ScopedGpuEvent&) = delete;
        ScopedGpuEvent& operator=(const ScopedGpuEvent&) = delete;

    private:
        CommandContext& context_;
    };
}
