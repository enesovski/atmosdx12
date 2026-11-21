#include "scene/Scene.h"
#include "core/Assert.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace atmos
{
    XMMATRIX Transform::ToMatrix() const
    {
        const XMVECTOR rotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotationDegrees.x),
                                                                   XMConvertToRadians(rotationDegrees.y),
                                                                   XMConvertToRadians(rotationDegrees.z));

        return XMMatrixScaling(scale.x, scale.y, scale.z) * XMMatrixRotationQuaternion(rotation) *
               XMMatrixTranslation(position.x, position.y, position.z);
    }

    Transform Transform::FromMatrix(FXMMATRIX matrix)
    {
        XMVECTOR scale, rotation, translation;
        XMMatrixDecompose(&scale, &rotation, &translation, matrix);

        // ToMatrix's rotation is Rz * Rx * Ry (row vectors), so
        // m21 = -sin(pitch), m20 = cos(pitch) sin(yaw), m22 = cos(pitch) cos(yaw),
        // m01 = sin(roll) cos(pitch), m11 = cos(roll) cos(pitch).
        XMFLOAT3X3 r;
        XMStoreFloat3x3(&r, XMMatrixRotationQuaternion(rotation));

        const float pitch = std::asin(std::clamp(-r._32, -1.0f, 1.0f));
        float yaw, roll;

        if (std::abs(r._32) < 0.9999f)
        {
            yaw = std::atan2(r._31, r._33);
            roll = std::atan2(r._12, r._22);
        }
        else
        {
            // Gimbal lock: yaw and roll share an axis, so put it all into yaw.
            yaw = std::atan2(-r._13, r._11);
            roll = 0.0f;
        }

        Transform transform;
        XMStoreFloat3(&transform.position, translation);
        XMStoreFloat3(&transform.scale, scale);
        transform.rotationDegrees = { XMConvertToDegrees(pitch), XMConvertToDegrees(yaw), XMConvertToDegrees(roll) };
        return transform;
    }

    EntityId Scene::CreateEntity(std::string name, EntityId parent)
    {
        ATMOS_ASSERT(parent == InvalidEntity || parent < entities_.size(), "Parent must already exist");

        entities_.push_back({ .name = std::move(name), .parent = parent });
        return static_cast<EntityId>(entities_.size() - 1);
    }

    bool Scene::IsDescendantOf(EntityId id, EntityId ancestor) const
    {
        for (EntityId current = id; current != InvalidEntity; current = entities_[current].parent)
        {
            if (current == ancestor)
                return true;
        }

        return false;
    }

    std::vector<EntityId> Scene::DestroyEntities(std::span<const EntityId> ids)
    {
        std::vector<bool> removed(entities_.size(), false);

        for (EntityId id : ids)
        {
            if (IsValid(id))
                removed[id] = true;
        }

        // Parents come first, so one pass propagates removal to every descendant.
        for (size_t i = 0; i < entities_.size(); ++i)
        {
            if (entities_[i].parent != InvalidEntity && removed[entities_[i].parent])
                removed[i] = true;
        }

        std::vector<EntityId> remap(entities_.size(), InvalidEntity);
        std::vector<Entity> kept;
        kept.reserve(entities_.size());

        for (size_t i = 0; i < entities_.size(); ++i)
        {
            if (removed[i])
                continue;

            remap[i] = static_cast<EntityId>(kept.size());
            kept.push_back(std::move(entities_[i]));
        }

        for (Entity& entity : kept)
        {
            if (entity.parent != InvalidEntity)
                entity.parent = remap[entity.parent];
        }

        entities_ = std::move(kept);
        return remap;
    }

    EntityId Scene::DuplicateEntity(EntityId id, std::vector<EntityId>& remap)
    {
        ATMOS_ASSERT(IsValid(id), "Invalid entity");

        // Descendants follow the entity in id order, so copying in order keeps parents first.
        std::vector<EntityId> subtree;

        for (EntityId i = id; i < entities_.size(); ++i)
        {
            if (IsDescendantOf(i, id))
                subtree.push_back(i);
        }

        std::vector<EntityId> copies(entities_.size(), InvalidEntity);

        for (EntityId source : subtree)
        {
            Entity copy = entities_[source];

            if (source == id)
                copy.name += " (Copy)";
            else
                copy.parent = copies[copy.parent];

            copies[source] = static_cast<EntityId>(entities_.size());
            entities_.push_back(std::move(copy));
        }

        // Sorting moves the copies directly under their parent, after the existing siblings.
        const EntityId copyId = copies[id];
        remap = SortHierarchy();
        return remap[copyId];
    }

    std::vector<EntityId> Scene::SetParent(EntityId id, EntityId parent)
    {
        ATMOS_ASSERT(IsValid(id), "Invalid entity");

        if (entities_[id].parent == parent || (parent != InvalidEntity && IsDescendantOf(parent, id)))
            return {};

        const std::vector<XMFLOAT4X4> world = ComputeWorldMatrices();
        XMMATRIX local = XMLoadFloat4x4(&world[id]);

        if (parent != InvalidEntity)
            local = local * XMMatrixInverse(nullptr, XMLoadFloat4x4(&world[parent]));

        entities_[id].transform = Transform::FromMatrix(local);
        entities_[id].parent = parent;
        return SortHierarchy();
    }

    std::vector<EntityId> Scene::SortHierarchy()
    {
        const size_t count = entities_.size();
        std::vector<std::vector<EntityId>> children(count);
        std::vector<EntityId> roots;

        for (EntityId i = 0; i < count; ++i)
            (entities_[i].parent == InvalidEntity ? roots : children[entities_[i].parent]).push_back(i);

        std::vector<EntityId> order;
        order.reserve(count);
        std::vector<EntityId> stack(roots.rbegin(), roots.rend());

        while (!stack.empty())
        {
            const EntityId id = stack.back();
            stack.pop_back();
            order.push_back(id);
            stack.insert(stack.end(), children[id].rbegin(), children[id].rend());
        }

        std::vector<EntityId> remap(count, InvalidEntity);

        for (EntityId newId = 0; newId < count; ++newId)
            remap[order[newId]] = newId;

        std::vector<Entity> sorted;
        sorted.reserve(count);

        for (EntityId oldId : order)
        {
            Entity& entity = sorted.emplace_back(std::move(entities_[oldId]));

            if (entity.parent != InvalidEntity)
                entity.parent = remap[entity.parent];
        }

        entities_ = std::move(sorted);
        return remap;
    }

    std::vector<XMFLOAT4X4> Scene::ComputeWorldMatrices() const
    {
        std::vector<XMFLOAT4X4> world(entities_.size());

        // Parents always have lower ids, so one forward pass resolves the hierarchy.
        for (size_t i = 0; i < entities_.size(); ++i)
        {
            XMMATRIX matrix = entities_[i].transform.ToMatrix();

            if (entities_[i].parent != InvalidEntity)
                matrix = matrix * XMLoadFloat4x4(&world[entities_[i].parent]);

            XMStoreFloat4x4(&world[i], matrix);
        }

        return world;
    }

    std::vector<bool> Scene::ComputeActiveInHierarchy() const
    {
        std::vector<bool> active(entities_.size());

        for (size_t i = 0; i < entities_.size(); ++i)
        {
            const EntityId parent = entities_[i].parent;
            active[i] = entities_[i].active && (parent == InvalidEntity || active[parent]);
        }

        return active;
    }
}
