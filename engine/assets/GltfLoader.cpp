#include "assets/GltfLoader.h"
#include "core/Log.h"
#include "core/StringUtils.h"

#include <cgltf.h>

#include <format>
#include <memory>
#include <stdexcept>
#include <utility>

namespace atmos
{
    namespace
    {
        using GltfDataPtr = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>;

        const cgltf_accessor* FindAttribute(const cgltf_primitive& primitive, cgltf_attribute_type type)
        {
            for (cgltf_size i = 0; i < primitive.attributes_count; ++i)
            {
                const cgltf_attribute& attribute = primitive.attributes[i];

                if (attribute.type == type && attribute.index == 0)
                    return attribute.data;
            }

            return nullptr;
        }

        DirectX::XMFLOAT4 GetBaseColor(const cgltf_primitive& primitive)
        {
            const cgltf_material* material = primitive.material;

            if (!material || !material->has_pbr_metallic_roughness)
                return { 1.0f, 1.0f, 1.0f, 1.0f };

            const cgltf_float* factor = material->pbr_metallic_roughness.base_color_factor;
            return { factor[0], factor[1], factor[2], factor[3] };
        }

        // Appends one triangle primitive as a submesh. Z is mirrored (right- to left-handed).
        void AppendPrimitive(MeshData& mesh, const cgltf_primitive& primitive)
        {
            const cgltf_accessor* positions = FindAttribute(primitive, cgltf_attribute_type_position);

            if (primitive.type != cgltf_primitive_type_triangles || !positions)
            {
                log::Warning("glTF: skipping a non-triangle primitive in mesh '{}'", mesh.name);
                return;
            }

            const cgltf_accessor* normals = FindAttribute(primitive, cgltf_attribute_type_normal);
            const cgltf_accessor* uvs = FindAttribute(primitive, cgltf_attribute_type_texcoord);

            if (!normals)
                log::Warning("glTF: mesh '{}' has no normals; lighting will be flat", mesh.name);

            const uint32_t baseVertex = static_cast<uint32_t>(mesh.positions.size());

            for (cgltf_size i = 0; i < positions->count; ++i)
            {
                float position[3]{};
                float normal[3]{ 0.0f, 1.0f, 0.0f };
                float uv[2]{};

                cgltf_accessor_read_float(positions, i, position, 3);

                if (normals)
                    cgltf_accessor_read_float(normals, i, normal, 3);

                if (uvs)
                    cgltf_accessor_read_float(uvs, i, uv, 2);

                mesh.positions.push_back({ position[0], position[1], -position[2] });
                mesh.attributes.push_back({ { normal[0], normal[1], -normal[2] }, { uv[0], uv[1] } });
            }

            Submesh submesh;
            submesh.indexOffset = static_cast<uint32_t>(mesh.indices.size());
            submesh.baseColor = GetBaseColor(primitive);

            if (primitive.indices)
            {
                for (cgltf_size i = 0; i < primitive.indices->count; ++i)
                    mesh.indices.push_back(baseVertex + static_cast<uint32_t>(cgltf_accessor_read_index(primitive.indices, i)));
            }
            else
            {
                for (cgltf_size i = 0; i < positions->count; ++i)
                    mesh.indices.push_back(baseVertex + static_cast<uint32_t>(i));
            }

            submesh.indexCount = static_cast<uint32_t>(mesh.indices.size()) - submesh.indexOffset;

            // Mirroring Z keeps the projected image identical, so glTF's counter-clockwise
            // front faces stay counter-clockwise on screen; the engine expects clockwise.
            for (size_t i = submesh.indexOffset; i + 2 < mesh.indices.size(); i += 3)
                std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);

            mesh.submeshes.push_back(submesh);
        }

        // glTF stores column-major matrices for column vectors; that memory layout is exactly a
        // row-major matrix for row vectors. Mirroring Z on both sides (S * M * S) negates every
        // element in row 2 or column 2, except the shared [2][2].
        DirectX::XMFLOAT4X4 ToEngineSpace(const cgltf_float (&gltfMatrix)[16])
        {
            DirectX::XMFLOAT4X4 matrix(gltfMatrix);

            for (int i = 0; i < 4; ++i)
            {
                if (i == 2)
                    continue;

                matrix.m[2][i] = -matrix.m[2][i];
                matrix.m[i][2] = -matrix.m[i][2];
            }

            return matrix;
        }
    }

    ModelData LoadGltfModel(const std::filesystem::path& path)
    {
        const std::string pathUtf8 = ToUtf8(path.wstring());
        const cgltf_options options{};

        cgltf_data* rawData = nullptr;

        if (cgltf_parse_file(&options, pathUtf8.c_str(), &rawData) != cgltf_result_success)
            throw std::runtime_error("Failed to parse glTF file: " + pathUtf8);

        const GltfDataPtr data(rawData, &cgltf_free);

        if (cgltf_load_buffers(&options, data.get(), pathUtf8.c_str()) != cgltf_result_success)
            throw std::runtime_error("Failed to load glTF buffers: " + pathUtf8);

        if (cgltf_validate(data.get()) != cgltf_result_success)
            throw std::runtime_error("Invalid glTF file: " + pathUtf8);

        ModelData model;
        model.meshes.reserve(data->meshes_count);

        for (cgltf_size m = 0; m < data->meshes_count; ++m)
        {
            const cgltf_mesh& gltfMesh = data->meshes[m];

            MeshData& mesh = model.meshes.emplace_back();
            mesh.name = gltfMesh.name ? gltfMesh.name : std::format("Mesh {}", m);

            for (cgltf_size p = 0; p < gltfMesh.primitives_count; ++p)
                AppendPrimitive(mesh, gltfMesh.primitives[p]);
        }

        for (cgltf_size n = 0; n < data->nodes_count; ++n)
        {
            const cgltf_node& node = data->nodes[n];

            if (!node.mesh)
                continue;

            cgltf_float world[16];
            cgltf_node_transform_world(&node, world);

            model.instances.push_back({ static_cast<uint32_t>(node.mesh - data->meshes), ToEngineSpace(world) });
        }

        log::Info("Loaded glTF '{}': {} meshes, {} instances", path.filename().string(), model.meshes.size(),
                  model.instances.size());
        return model;
    }
}
