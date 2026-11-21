#pragma once

#include "core/Handle.h"

#include <DirectXMath.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace atmos
{
    using EntityId = uint32_t;
    inline constexpr EntityId InvalidEntity = UINT32_MAX;

    // Relative to the parent. Euler degrees, applied roll (Z), pitch (X), yaw (Y).
    struct Transform
    {
        DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 rotationDegrees{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };

        DirectX::XMMATRIX ToMatrix() const;
    };

    // `handle` is assigned at runtime by the asset manager.
    struct ModelComponent
    {
        std::filesystem::path path;  // absolute at runtime; relative to the scene file on disk
        ModelHandle handle;
    };

    struct Entity
    {
        std::string name;
        Transform transform;
        EntityId parent = InvalidEntity;
        std::optional<ModelComponent> model;
    };

    struct SceneEnvironment
    {
        float sunAzimuthDegrees = 30.0f;
        float sunElevationDegrees = 26.1f;
        float sunIntensity = 1.0f;
        float exposure = 1.0f;
    };

    // Where the viewer starts; saved with the scene.
    struct SceneCamera
    {
        DirectX::XMFLOAT3 position{ 0.0f, 2.0f, -8.0f };
        float yawDegrees = 0.0f;
        float pitchDegrees = 0.0f;
        float fovDegrees = 65.0f;
    };

    // Graphics-free world description; converted to a RenderScene every frame.
    class Scene
    {
    public:
        // Parents must be created before their children.
        EntityId CreateEntity(std::string name, EntityId parent = InvalidEntity);

        Entity& GetEntity(EntityId id) { return entities_[id]; }
        const Entity& GetEntity(EntityId id) const { return entities_[id]; }
        std::span<Entity> GetEntities() { return entities_; }
        std::span<const Entity> GetEntities() const { return entities_; }

        // Indexed by EntityId.
        std::vector<DirectX::XMFLOAT4X4> ComputeWorldMatrices() const;

        SceneEnvironment environment;
        SceneCamera camera;

    private:
        std::vector<Entity> entities_;
    };
}
