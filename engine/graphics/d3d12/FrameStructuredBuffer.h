#pragma once

#include "graphics/d3d12/Buffer.h"
#include "graphics/d3d12/GraphicsConfig.h"
#include "core/Log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <span>
#include <string>
#include <type_traits>

namespace atmos
{
    // A StructuredBuffer<T> rewritten every frame: one persistently mapped upload buffer per frame
    // slot, each with its own bindless SRV. Writing slot N is safe once FrameScheduler waited on it.
    template <typename T>
    class FrameStructuredBuffer
    {
        static_assert(std::is_trivially_copyable_v<T>);

    public:
        FrameStructuredBuffer(GraphicsDevice& device, uint32_t capacity, std::wstring_view name)
            : capacity_(capacity)
        {
            for (uint32_t i = 0; i < GraphicsConfig::FramesInFlight; ++i)
            {
                buffers_[i] = Buffer(device, {
                    .size = static_cast<uint64_t>(capacity) * sizeof(T),
                    .stride = sizeof(T),
                    .memory = BufferMemory::Upload,
                    .debugName = std::format(L"{} {}", name, i),
                });
            }
        }

        // Returns the number of elements written (clamped to the capacity).
        uint32_t Write(uint32_t frameIndex, std::span<const T> elements)
        {
            const auto count = static_cast<uint32_t>(std::min<size_t>(elements.size(), capacity_));

            if (count < elements.size() && !overflowReported_)
            {
                log::Warning("{} elements exceed a frame buffer capacity of {}", elements.size(), capacity_);
                overflowReported_ = true;
            }

            std::memcpy(buffers_[frameIndex].GetMappedData(), elements.data(), count * sizeof(T));
            return count;
        }

        T* GetMappedData(uint32_t frameIndex) const
        {
            return reinterpret_cast<T*>(buffers_[frameIndex].GetMappedData());
        }

        uint32_t GetSrvIndex(uint32_t frameIndex) const { return buffers_[frameIndex].GetSrvIndex(); }
        uint32_t GetCapacity() const { return capacity_; }

    private:
        std::array<Buffer, GraphicsConfig::FramesInFlight> buffers_;
        uint32_t capacity_;
        bool overflowReported_ = false;
    };
}
