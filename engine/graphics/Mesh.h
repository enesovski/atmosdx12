#pragma once

#include "graphics/MeshData.h"
#include "graphics/d3d12/Buffer.h"

#include <DirectXCollision.h>

#include <span>
#include <string>
#include <vector>

namespace atmos
{
    class GraphicsDevice;
    class UploadQueue;

    // GPU mesh: separate position and attribute streams (read by vertex pulling), a 32-bit
    // index buffer, the submesh table and local-space bounds for culling.
    class Mesh
    {
    public:
        // Records uploads; the mesh may be drawn once the upload queue completed them.
        Mesh(GraphicsDevice& device, UploadQueue& uploadQueue, const MeshData& data);

        const Buffer& GetPositionBuffer() const { return positions_; }
        const Buffer& GetAttributeBuffer() const { return attributes_; }
        const Buffer& GetIndexBuffer() const { return indices_; }
        std::span<const Submesh> GetSubmeshes() const { return submeshes_; }
        const DirectX::BoundingBox& GetLocalBounds() const { return localBounds_; }
        const std::string& GetName() const { return name_; }

    private:
        Buffer positions_;
        Buffer attributes_;
        Buffer indices_;
        std::vector<Submesh> submeshes_;
        DirectX::BoundingBox localBounds_;
        std::string name_;
    };
}
