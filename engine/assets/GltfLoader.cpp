#include "assets/GltfLoader.h"
#include "core/Log.h"
#include "core/StringUtils.h"

#include <cgltf.h>

#include <cmath>
#include <format>
#include <memory>
#include <stdexcept>
#include <utility>

using namespace DirectX;

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

        std::filesystem::path ResolveTexturePath(const cgltf_texture_view& view, const std::filesystem::path& baseDirectory)
        {
            if (!view.texture || !view.texture->image)
                return {};

            const cgltf_image* image = view.texture->image;

            if (!image->uri || std::string_view(image->uri).starts_with("data:"))
            {
                log::Warning("glTF: embedded image '{}' is not supported yet", image->name ? image->name : "");
                return {};
            }

            std::string uri = image->uri;
            cgltf_decode_uri(uri.data());
            uri.resize(std::char_traits<char>::length(uri.c_str()));

            return (baseDirectory / std::filesystem::path(ToWide(uri))).lexically_normal();
        }

        MaterialData ConvertMaterial(const cgltf_material& material, const std::filesystem::path& baseDirectory)
        {
            MaterialData result;
            result.name = material.name ? material.name : "";
            result.alphaCutoff = material.alpha_cutoff;
            result.doubleSided = material.double_sided;
            result.emissiveFactor = { material.emissive_factor[0], material.emissive_factor[1],
                                      material.emissive_factor[2] };

            switch (material.alpha_mode)
            {
                case cgltf_alpha_mode_mask:  result.alphaMode = AlphaMode::Mask; break;
                case cgltf_alpha_mode_blend: result.alphaMode = AlphaMode::Blend; break;
                default:                     result.alphaMode = AlphaMode::Opaque; break;
            }

            auto setTexture = [&](MaterialTextureSlot slot, const cgltf_texture_view& view) {
                result.textures[static_cast<size_t>(slot)] = ResolveTexturePath(view, baseDirectory);
            };

            if (material.has_pbr_metallic_roughness)
            {
                const cgltf_pbr_metallic_roughness& pbr = material.pbr_metallic_roughness;
                result.baseColorFactor = { pbr.base_color_factor[0], pbr.base_color_factor[1],
                                           pbr.base_color_factor[2], pbr.base_color_factor[3] };
                result.metallicFactor = pbr.metallic_factor;
                result.roughnessFactor = pbr.roughness_factor;
                setTexture(MaterialTextureSlot::BaseColor, pbr.base_color_texture);
                setTexture(MaterialTextureSlot::MetallicRoughness, pbr.metallic_roughness_texture);
            }

            setTexture(MaterialTextureSlot::Normal, material.normal_texture);
            setTexture(MaterialTextureSlot::Occlusion, material.occlusion_texture);
            setTexture(MaterialTextureSlot::Emissive, material.emissive_texture);
            return result;
        }

        // Per-triangle UV-gradient tangents (Lengyel), accumulated per vertex. Used only when
        // the file has none; MikkTSpace would match authoring tools exactly.
        void GenerateTangents(MeshData& mesh, size_t firstVertex, size_t firstIndex)
        {
            const size_t vertexCount = mesh.positions.size() - firstVertex;
            std::vector<XMFLOAT3> tangents(vertexCount, { 0, 0, 0 });
            std::vector<XMFLOAT3> bitangents(vertexCount, { 0, 0, 0 });

            for (size_t i = firstIndex; i + 2 < mesh.indices.size(); i += 3)
            {
                const uint32_t v[3] = { mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2] };
                const XMVECTOR p0 = XMLoadFloat3(&mesh.positions[v[0]]);
                const XMVECTOR e1 = XMLoadFloat3(&mesh.positions[v[1]]) - p0;
                const XMVECTOR e2 = XMLoadFloat3(&mesh.positions[v[2]]) - p0;
                const XMFLOAT2 uv0 = mesh.attributes[v[0]].uv;
                const float du1 = mesh.attributes[v[1]].uv.x - uv0.x, dv1 = mesh.attributes[v[1]].uv.y - uv0.y;
                const float du2 = mesh.attributes[v[2]].uv.x - uv0.x, dv2 = mesh.attributes[v[2]].uv.y - uv0.y;
                const float determinant = du1 * dv2 - du2 * dv1;

                if (std::abs(determinant) < 1e-12f)
                    continue;

                const float r = 1.0f / determinant;
                const XMVECTOR tangent = (e1 * dv2 - e2 * dv1) * r;
                const XMVECTOR bitangent = (e2 * du1 - e1 * du2) * r;

                for (uint32_t vertex : v)
                {
                    XMFLOAT3& t = tangents[vertex - firstVertex];
                    XMFLOAT3& b = bitangents[vertex - firstVertex];
                    XMStoreFloat3(&t, XMLoadFloat3(&t) + tangent);
                    XMStoreFloat3(&b, XMLoadFloat3(&b) + bitangent);
                }
            }

            for (size_t i = 0; i < vertexCount; ++i)
            {
                VertexAttributes& attributes = mesh.attributes[firstVertex + i];
                const XMVECTOR n = XMLoadFloat3(&attributes.normal);
                XMVECTOR t = XMLoadFloat3(&tangents[i]);
                t = XMVector3Normalize(t - n * XMVector3Dot(n, t));  // Gram-Schmidt

                if (XMVector3Equal(t, XMVectorZero()) || XMVector3IsNaN(t))
                    t = XMVector3Orthogonal(n);

                const float sign = XMVectorGetX(XMVector3Dot(XMVector3Cross(n, t), XMLoadFloat3(&bitangents[i]))) < 0.0f
                                       ? -1.0f : 1.0f;
                XMStoreFloat4(&attributes.tangent, XMVectorSetW(t, sign));
            }
        }

        // Appends one triangle primitive as a submesh, converted to engine space.
        void AppendPrimitive(MeshData& mesh, const cgltf_primitive& primitive, uint32_t materialIndex)
        {
            const cgltf_accessor* positions = FindAttribute(primitive, cgltf_attribute_type_position);

            if (primitive.type != cgltf_primitive_type_triangles || !positions)
            {
                log::Warning("glTF: skipping a non-triangle primitive in mesh '{}'", mesh.name);
                return;
            }

            const cgltf_accessor* normals = FindAttribute(primitive, cgltf_attribute_type_normal);
            const cgltf_accessor* tangents = FindAttribute(primitive, cgltf_attribute_type_tangent);
            const cgltf_accessor* uvs = FindAttribute(primitive, cgltf_attribute_type_texcoord);

            if (!normals)
                log::Warning("glTF: mesh '{}' has no normals; lighting will be flat", mesh.name);

            const size_t baseVertex = mesh.positions.size();

            for (cgltf_size i = 0; i < positions->count; ++i)
            {
                float position[3]{};
                float normal[3]{ 0.0f, 1.0f, 0.0f };
                float tangent[4]{ 1.0f, 0.0f, 0.0f, 1.0f };
                float uv[2]{};

                cgltf_accessor_read_float(positions, i, position, 3);

                if (normals)
                    cgltf_accessor_read_float(normals, i, normal, 3);

                if (tangents)
                    cgltf_accessor_read_float(tangents, i, tangent, 4);

                if (uvs)
                    cgltf_accessor_read_float(uvs, i, uv, 2);

                // Mirroring Z is an improper transform, so the bitangent sign flips too.
                mesh.positions.push_back({ position[0], position[1], -position[2] });
                mesh.attributes.push_back({ { normal[0], normal[1], -normal[2] },
                                            { tangent[0], tangent[1], -tangent[2], -tangent[3] },
                                            { uv[0], uv[1] } });
            }

            Submesh submesh;
            submesh.indexOffset = static_cast<uint32_t>(mesh.indices.size());
            submesh.materialIndex = materialIndex;

            if (primitive.indices)
            {
                for (cgltf_size i = 0; i < primitive.indices->count; ++i)
                {
                    const auto index = static_cast<uint32_t>(cgltf_accessor_read_index(primitive.indices, i));
                    mesh.indices.push_back(static_cast<uint32_t>(baseVertex) + index);
                }
            }
            else
            {
                for (cgltf_size i = 0; i < positions->count; ++i)
                    mesh.indices.push_back(static_cast<uint32_t>(baseVertex + i));
            }

            submesh.indexCount = static_cast<uint32_t>(mesh.indices.size()) - submesh.indexOffset;

            // Mirroring keeps the projected image identical, so glTF's counter-clockwise front
            // faces stay counter-clockwise on screen; the engine expects clockwise.
            for (size_t i = submesh.indexOffset; i + 2 < mesh.indices.size(); i += 3)
                std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);

            if (!tangents)
                GenerateTangents(mesh, baseVertex, submesh.indexOffset);

            mesh.submeshes.push_back(submesh);
        }

        // glTF stores column-major matrices for column vectors; that memory layout is exactly a
        // row-major matrix for row vectors. Mirroring Z on both sides (S * M * S) negates every
        // element in row 2 or column 2, except the shared [2][2].
        XMFLOAT4X4 ToEngineSpace(const cgltf_float (&gltfMatrix)[16])
        {
            XMFLOAT4X4 matrix(gltfMatrix);

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

    ModelData ImportGltfModel(const std::filesystem::path& path)
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

        const std::filesystem::path baseDirectory = path.parent_path();
        ModelData model;

        // Index materials.size() is the default material for primitives without one.
        for (cgltf_size m = 0; m < data->materials_count; ++m)
            model.materials.push_back(ConvertMaterial(data->materials[m], baseDirectory));

        const auto defaultMaterial = static_cast<uint32_t>(model.materials.size());
        bool usesDefaultMaterial = false;

        for (cgltf_size m = 0; m < data->meshes_count; ++m)
        {
            const cgltf_mesh& gltfMesh = data->meshes[m];

            MeshData& mesh = model.meshes.emplace_back();
            mesh.name = gltfMesh.name ? gltfMesh.name : std::format("Mesh {}", m);

            for (cgltf_size p = 0; p < gltfMesh.primitives_count; ++p)
            {
                const cgltf_material* material = gltfMesh.primitives[p].material;
                usesDefaultMaterial |= material == nullptr;

                const uint32_t materialIndex =
                    material ? static_cast<uint32_t>(material - data->materials) : defaultMaterial;
                AppendPrimitive(mesh, gltfMesh.primitives[p], materialIndex);
            }
        }

        if (usesDefaultMaterial)
            model.materials.push_back({ .name = "Default" });

        for (cgltf_size n = 0; n < data->nodes_count; ++n)
        {
            const cgltf_node& node = data->nodes[n];

            if (!node.mesh)
                continue;

            cgltf_float world[16];
            cgltf_node_transform_world(&node, world);
            model.instances.push_back({ static_cast<uint32_t>(node.mesh - data->meshes), ToEngineSpace(world) });
        }

        log::Info("Imported glTF '{}': {} meshes, {} materials, {} instances", path.filename().string(),
                  model.meshes.size(), model.materials.size(), model.instances.size());
        return model;
    }
}
