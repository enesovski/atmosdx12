#include "assets/ModelCooker.h"
#include "assets/AssetCache.h"
#include "assets/GltfLoader.h"
#include "core/Log.h"
#include "core/StringUtils.h"

#include <cstdint>
#include <format>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace atmos
{
    namespace
    {
        // Bump when ModelData or the importer changes so old cache files are re-cooked.
        constexpr uint32_t FormatVersion = 1;
        constexpr uint32_t Magic = 0x4C444D41;  // "AMDL"

        class BinaryWriter
        {
        public:
            explicit BinaryWriter(const std::filesystem::path& path) : stream_(path, std::ios::binary)
            {
                if (!stream_)
                    throw std::runtime_error("Failed to create " + ToUtf8(path.wstring()));
            }

            template <typename T>
            void Write(const T& value)
            {
                static_assert(std::is_trivially_copyable_v<T>);
                stream_.write(reinterpret_cast<const char*>(&value), sizeof(T));
            }

            template <typename T>
            void WriteVector(const std::vector<T>& values)
            {
                static_assert(std::is_trivially_copyable_v<T>);
                Write(static_cast<uint64_t>(values.size()));
                stream_.write(reinterpret_cast<const char*>(values.data()), values.size() * sizeof(T));
            }

            void WriteString(const std::string& text)
            {
                Write(static_cast<uint64_t>(text.size()));
                stream_.write(text.data(), text.size());
            }

            bool Good() const { return stream_.good(); }

        private:
            std::ofstream stream_;
        };

        class BinaryReader
        {
        public:
            explicit BinaryReader(const std::filesystem::path& path) : stream_(path, std::ios::binary)
            {
                if (!stream_)
                    throw std::runtime_error("Failed to open " + ToUtf8(path.wstring()));
            }

            template <typename T>
            T Read()
            {
                static_assert(std::is_trivially_copyable_v<T>);
                T value{};
                Fill(&value, sizeof(T));
                return value;
            }

            template <typename T>
            std::vector<T> ReadVector()
            {
                std::vector<T> values(Read<uint64_t>());
                Fill(values.data(), values.size() * sizeof(T));
                return values;
            }

            std::string ReadString()
            {
                std::string text(Read<uint64_t>(), '\0');
                Fill(text.data(), text.size());
                return text;
            }

        private:
            void Fill(void* destination, size_t size)
            {
                if (!stream_.read(static_cast<char*>(destination), size))
                    throw std::runtime_error("Cooked model is truncated");
            }

            std::ifstream stream_;
        };

        void WriteModel(const ModelData& model, const std::filesystem::path& path)
        {
            BinaryWriter writer(path);
            writer.Write(Magic);
            writer.Write(FormatVersion);

            writer.Write(static_cast<uint64_t>(model.meshes.size()));
            for (const MeshData& mesh : model.meshes)
            {
                writer.WriteString(mesh.name);
                writer.WriteVector(mesh.positions);
                writer.WriteVector(mesh.attributes);
                writer.WriteVector(mesh.indices);
                writer.WriteVector(mesh.submeshes);
            }

            writer.Write(static_cast<uint64_t>(model.materials.size()));
            for (const MaterialData& material : model.materials)
            {
                writer.WriteString(material.name);
                writer.Write(material.baseColorFactor);
                writer.Write(material.emissiveFactor);
                writer.Write(material.metallicFactor);
                writer.Write(material.roughnessFactor);
                writer.Write(material.alphaCutoff);
                writer.Write(material.alphaMode);
                writer.Write(material.doubleSided);

                for (const std::filesystem::path& texture : material.textures)
                    writer.WriteString(ToUtf8(texture.wstring()));
            }

            writer.WriteVector(model.instances);

            if (!writer.Good())
                throw std::runtime_error("Failed to write cooked model " + ToUtf8(path.wstring()));
        }

        ModelData ReadModel(const std::filesystem::path& path)
        {
            BinaryReader reader(path);

            if (reader.Read<uint32_t>() != Magic || reader.Read<uint32_t>() != FormatVersion)
                throw std::runtime_error("Cooked model has an unknown format");

            ModelData model;
            model.meshes.resize(reader.Read<uint64_t>());

            for (MeshData& mesh : model.meshes)
            {
                mesh.name = reader.ReadString();
                mesh.positions = reader.ReadVector<DirectX::XMFLOAT3>();
                mesh.attributes = reader.ReadVector<VertexAttributes>();
                mesh.indices = reader.ReadVector<uint32_t>();
                mesh.submeshes = reader.ReadVector<Submesh>();
            }

            model.materials.resize(reader.Read<uint64_t>());

            for (MaterialData& material : model.materials)
            {
                material.name = reader.ReadString();
                material.baseColorFactor = reader.Read<DirectX::XMFLOAT4>();
                material.emissiveFactor = reader.Read<DirectX::XMFLOAT3>();
                material.metallicFactor = reader.Read<float>();
                material.roughnessFactor = reader.Read<float>();
                material.alphaCutoff = reader.Read<float>();
                material.alphaMode = reader.Read<AlphaMode>();
                material.doubleSided = reader.Read<bool>();

                for (std::filesystem::path& texture : material.textures)
                    texture = ToWide(reader.ReadString());
            }

            model.instances = reader.ReadVector<ModelInstance>();
            return model;
        }
    }

    ModelData LoadCookedModel(const std::filesystem::path& source, const std::filesystem::path& cacheDirectory)
    {
        const std::filesystem::path cooked =
            GetCookedPath(cacheDirectory, source, std::format("model-v{}", FormatVersion), ".amodel");

        if (IsCookedFileValid(cooked, source))
        {
            try
            {
                return ReadModel(cooked);
            }
            catch (const std::exception& e)
            {
                log::Warning("Re-cooking {}: {}", source.filename().string(), e.what());
            }
        }

        ModelData model = ImportGltfModel(source);

        std::filesystem::path temporary = cooked;
        temporary += std::format(L".{}.tmp", std::hash<std::thread::id>{}(std::this_thread::get_id()));
        WriteModel(model, temporary);
        CommitCookedFile(temporary, cooked);

        return model;
    }
}
