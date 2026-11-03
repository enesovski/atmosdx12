#include "CelestialManager.h"

#include <cmath>
#include <numbers>

void CelestialManager::Update(float timeOfDay)
{
    // 06:00 is sunrise, 12:00 noon, and 18:00 sunset.
    const float angle = (timeOfDay - 6.0f) * std::numbers::pi_v<float> / 12.0f;
    const float horizontal = std::cos(angle);

    float x = 0.5f * horizontal;
    float y = 0.85f * std::sin(angle);
    float z = 0.866f * horizontal;

    const float invLength = 1.0f / std::sqrt(x * x + y * y + z * z);
    x *= invLength;
    y *= invLength;
    z *= invLength;

    state_.sunDirection = { x, y, z };
    state_.moonDirection = { -x, -y, -z };
}