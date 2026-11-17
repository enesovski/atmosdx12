#pragma once

#include <array>
#include <cstdint>

namespace atmos
{
    struct MouseDelta
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    // Accumulates window input between frames; the app consumes it once per frame.
    // Keys use Win32 virtual-key codes ('W', VK_SHIFT, ...).
    class Input
    {
    public:
        void BeginLook(int x, int y)
        {
            looking_ = true;
            lastX_ = x;
            lastY_ = y;
        }

        void EndLook() { looking_ = false; }

        void OnMouseMove(int x, int y)
        {
            if (!looking_)
                return;

            lookDelta_.x += static_cast<float>(x - lastX_);
            lookDelta_.y += static_cast<float>(y - lastY_);
            lastX_ = x;
            lastY_ = y;
        }

        void AddWheelSteps(float steps) { wheelSteps_ += steps; }

        void SetKeyDown(uint32_t key, bool down)
        {
            if (key < keys_.size())
                keys_[key] = down;
        }

        bool IsKeyDown(uint32_t key) const { return key < keys_.size() && keys_[key]; }

        // Focus loss means key-up messages may never arrive.
        void ReleaseAllKeys()
        {
            keys_.fill(false);
            looking_ = false;
        }

        MouseDelta ConsumeLookDelta()
        {
            const MouseDelta delta = lookDelta_;
            lookDelta_ = {};
            return delta;
        }

        float ConsumeWheelSteps()
        {
            const float steps = wheelSteps_;
            wheelSteps_ = 0.0f;
            return steps;
        }

    private:
        std::array<bool, 256> keys_{};
        bool looking_ = false;
        int lastX_ = 0;
        int lastY_ = 0;
        MouseDelta lookDelta_{};
        float wheelSteps_ = 0.0f;
    };
}
