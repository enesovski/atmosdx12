#include "scene/Scene.h"
#include "core/Assert.h"

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

    EntityId Scene::CreateEntity(std::string name, EntityId parent)
    {
        ATMOS_ASSERT(parent == InvalidEntity || parent < entities_.size(), "Parent must already exist");

        entities_.push_back({ .name = std::move(name), .parent = parent });
        return static_cast<EntityId>(entities_.size() - 1);
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
}
