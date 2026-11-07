#pragma once

#include <DirectXMath.h>

#include <cmath>
#include <numbers>

struct SkySettings
{
    float sunAzimuthDegrees = 30.0f;
    float sunElevationDegrees = 26.1f;
    float sunIntensity = 1.0f;
};

inline DirectX::XMFLOAT3 CalculateSunDirection(float azimuthDegrees, float elevationDegrees)
{
    constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0f;
    const float azimuth = azimuthDegrees * degreesToRadians;
    const float elevation = elevationDegrees * degreesToRadians;
    const float horizontal = std::cos(elevation);

    // +Y is up; azimuth turns from +Z toward +X, matching camera yaw.
    return { horizontal * std::sin(azimuth), std::sin(elevation),
             horizontal * std::cos(azimuth) };
}
