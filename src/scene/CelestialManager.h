#pragma once

#include <DirectXMath.h>

struct CelestialState
{
    DirectX::XMFLOAT3 sunDirection{};
    DirectX::XMFLOAT3 moonDirection{};
};

class CelestialManager
{
public:
    void Update(float timeOfDay);
    const CelestialState& GetState() const { return state_; }

private:
    CelestialState state_{};
};