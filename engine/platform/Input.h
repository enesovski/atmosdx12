#pragma once

#include <array>
#include <cstdint>
#include <utility>

namespace atmos
{
    enum class MouseButton : uint32_t
    {
        Left,
        Right,
        Middle,
        Count,
    };

    struct MouseDelta
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    // Input state between frames. Window writes it; the app reads it once per frame.
    // Keys are Win32 virtual-key codes ('W', VK_SHIFT, ...). Mouse motion is raw (unaccelerated).
    class Input
    {
    public:
        void OnKey(uint32_t key, bool down)
        {
            if (key >= keys_.size())
                return;

            if (down && !keys_[key])
                keyPresses_[key] = true;

            keys_[key] = down;
        }

        void OnMouseButton(MouseButton button, bool down)
        {
            const auto index = static_cast<size_t>(button);

            if (down && !buttons_[index])
                buttonPresses_[index] = true;

            buttons_[index] = down;
        }

        void OnMouseMove(float deltaX, float deltaY)
        {
            mouseDelta_.x += deltaX;
            mouseDelta_.y += deltaY;
        }

        void OnWheel(float steps) { wheelSteps_ += steps; }

        // Release events may never arrive after focus loss.
        void Reset() { *this = {}; }

        bool IsKeyDown(uint32_t key) const { return key < keys_.size() && keys_[key]; }
        bool IsMouseButtonDown(MouseButton button) const { return buttons_[static_cast<size_t>(button)]; }

        // True once per physical press (ignores key repeat).
        bool ConsumeKeyPress(uint32_t key) { return key < keyPresses_.size() && Consume(keyPresses_[key]); }
        bool ConsumeMouseButtonPress(MouseButton button) { return Consume(buttonPresses_[static_cast<size_t>(button)]); }

        MouseDelta ConsumeMouseDelta() { return std::exchange(mouseDelta_, {}); }
        float ConsumeWheelSteps() { return std::exchange(wheelSteps_, 0.0f); }

    private:
        static bool Consume(bool& flag) { return std::exchange(flag, false); }

        static constexpr size_t ButtonCount = static_cast<size_t>(MouseButton::Count);

        std::array<bool, 256> keys_{};
        std::array<bool, 256> keyPresses_{};
        std::array<bool, ButtonCount> buttons_{};
        std::array<bool, ButtonCount> buttonPresses_{};
        MouseDelta mouseDelta_{};
        float wheelSteps_ = 0.0f;
    };
}
