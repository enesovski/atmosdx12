
#pragma once

struct MouseDelta
{
    float x = 0.0f;
    float y = 0.0f;
};

class Input
{
public:
    void BeginLook(int x, int y)
    {
        looking_ = true;
        lastX_ = x;
        lastY_ = y;
    }

    void EndLook()
    {
        looking_ = false;
    }

    void OnMouseMove(int x, int y)
    {
        if (!looking_)
            return;

        lookDelta_.x += static_cast<float>(x - lastX_);
        lookDelta_.y += static_cast<float>(y - lastY_);

        lastX_ = x;
        lastY_ = y;
    }

    void AddWheelSteps(float steps)
    {
        wheelSteps_ += steps;
    }

    MouseDelta ConsumeLookDelta()
    {
        MouseDelta delta = lookDelta_;
        lookDelta_ = {};

        return delta;
    }

    float ConsumeWheelSteps()
    {
        float steps = wheelSteps_;
        wheelSteps_ = 0.0f;

        return steps;
    }

private:
    bool looking_ = false;

    int lastX_ = 0;
    int lastY_ = 0;

    MouseDelta lookDelta_{};
    float wheelSteps_ = 0.0f;
};
