#pragma once

#include "core/Handle.h"
#include "scene/RenderSettings.h"

#include <DirectXMath.h>

#include <array>
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

        // Inverse of ToMatrix for scale * rotation * translation matrices (no shear).
        static Transform FromMatrix(DirectX::FXMMATRIX matrix);
    };

    // `handle` is assigned at runtime by the asset manager.
    struct ModelComponent
    {
        std::filesystem::path path;  // absolute at runtime; relative to the scene file on disk
        ModelHandle handle;
    };

    enum class LightType : uint32_t
    {
        Point,
        Spot,
    };

    // Positioned by the entity; spot lights shine along the entity's +Z.
    struct LightComponent
    {
        LightType type = LightType::Point;
        DirectX::XMFLOAT3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 10.0f;  // luminous intensity (candela-like, same scale as the sun)
        float range = 10.0f;      // meters; light falls off smoothly to zero here
        float innerConeDegrees = 20.0f;
        float outerConeDegrees = 30.0f;
        bool castsShadows = false;
    };

    struct Entity
    {
        std::string name;
        Transform transform;
        EntityId parent = InvalidEntity;
        bool active = true;  // inactive entities and their children are not rendered
        std::optional<ModelComponent> model;
        std::optional<LightComponent> light;
    };

    struct SceneEnvironment
    {
        float sunAzimuthDegrees = 30.0f;
        float sunElevationDegrees = 26.1f;
        float sunIntensity = 8.0f;  // illuminance; local lights use the same scale
        float exposure = 1.0f;
    };

    // A point of view; the start camera and the bookmarks are saved with the scene.
    struct SceneCamera
    {
        DirectX::XMFLOAT3 position{ 0.0f, 2.0f, -8.0f };
        float yawDegrees = 0.0f;
        float pitchDegrees = 0.0f;
        float fovDegrees = 65.0f;
    };

    // Graphics-free world description; converted to a RenderScene every frame.
    // Entities are stored parents-first: an entity's parent always has a lower id. Structural
    // edits (destroy, reparent) keep that order and return how ids moved.
    class Scene
    {
    public:
        static constexpr size_t BookmarkCount = 9;

        // Parents must be created before their children.
        EntityId CreateEntity(std::string name, EntityId parent = InvalidEntity);

        Entity& GetEntity(EntityId id) { return entities_[id]; }
        const Entity& GetEntity(EntityId id) const { return entities_[id]; }
        std::span<Entity> GetEntities() { return entities_; }
        std::span<const Entity> GetEntities() const { return entities_; }
        size_t GetEntityCount() const { return entities_.size(); }
        bool IsValid(EntityId id) const { return id < entities_.size(); }

        // True if `ancestor` is `id` or one of its parents.
        bool IsDescendantOf(EntityId id, EntityId ancestor) const;

        // Removes the entities and all their descendants. Returns old id -> new id (InvalidEntity if removed).
        std::vector<EntityId> DestroyEntities(std::span<const EntityId> ids);

        // Copies the entity and its descendants as its last sibling. Returns the copy's id and old -> new ids.
        EntityId DuplicateEntity(EntityId id, std::vector<EntityId>& remap);

        // Reparents `id` (InvalidEntity = root), keeping its world transform. Fails on cycles.
        // Returns old id -> new id, or an empty vector if nothing changed.
        std::vector<EntityId> SetParent(EntityId id, EntityId parent);

        // Indexed by EntityId.
        std::vector<DirectX::XMFLOAT4X4> ComputeWorldMatrices() const;

        // An entity is active in the hierarchy if it and all its parents are active.
        std::vector<bool> ComputeActiveInHierarchy() const;

        SceneEnvironment environment;
        SceneCamera camera;
        RenderSettings renderSettings;
        std::array<std::optional<SceneCamera>, BookmarkCount> cameraBookmarks;

    private:
        // Restores parents-first order (depth-first, siblings in their current order).
        std::vector<EntityId> SortHierarchy();

        std::vector<Entity> entities_;
    };
}
