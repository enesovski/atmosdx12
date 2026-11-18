#pragma once

#include <cstdint>

namespace atmos
{
    // Typed index into a manager's table. The tag only keeps handle kinds apart.
    template <typename Tag>
    struct Handle
    {
        static constexpr uint32_t InvalidIndex = UINT32_MAX;
        uint32_t index = InvalidIndex;

        bool IsValid() const { return index != InvalidIndex; }
        bool operator==(const Handle&) const = default;
    };

    using ModelHandle = Handle<struct ModelHandleTag>;
}
