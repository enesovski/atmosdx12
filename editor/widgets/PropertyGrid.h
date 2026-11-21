#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <span>
#include <string>

namespace atmos::editor
{
    // Unreal Details-style two-column table: dim label on the left (resizable), value on the right,
    // and a reset arrow when the value differs from its default. Rows return true when the value
    // changed this frame; the caller marks the scene edited, and undo merges a whole drag into one step.
    class PropertyGrid
    {
    public:
        explicit PropertyGrid(const char* id);
        ~PropertyGrid();

        PropertyGrid(const PropertyGrid&) = delete;
        PropertyGrid& operator=(const PropertyGrid&) = delete;

        bool Float(const char* label, float& value, float defaultValue, float min, float max, const char* format = "%.2f",
                   float speed = 0.01f, const char* tooltip = nullptr);
        bool Slider(const char* label, float& value, float defaultValue, float min, float max, const char* format = "%.2f",
                    bool logarithmic = false, const char* tooltip = nullptr);
        bool UInt(const char* label, uint32_t& value, uint32_t defaultValue, uint32_t min, uint32_t max,
                  const char* tooltip = nullptr);
        bool Float3(const char* label, DirectX::XMFLOAT3& value, const DirectX::XMFLOAT3& defaultValue,
                    float speed = 0.01f, const char* format = "%.2f", const char* tooltip = nullptr);
        bool Color(const char* label, DirectX::XMFLOAT3& value, const DirectX::XMFLOAT3& defaultValue,
                   const char* tooltip = nullptr);
        bool Bool(const char* label, bool& value, bool defaultValue, const char* tooltip = nullptr);
        bool Enum(const char* label, uint32_t& value, std::span<const char* const> names, uint32_t defaultValue,
                  const char* tooltip = nullptr);
        // Discrete values shown by name (e.g. shadow-map resolutions).
        bool Choice(const char* label, uint32_t& value, std::span<const uint32_t> values, uint32_t defaultValue,
                    const char* tooltip = nullptr);
        bool Text(const char* label, std::string& value);
        void ReadOnly(const char* label, const char* format, ...);

    private:
        // Starts a row and returns the width left for the value; `modified` reserves the reset arrow.
        float BeginRow(const char* label, const char* tooltip);
        bool ResetButton(bool modified);

        bool visible_ = false;
    };
}
