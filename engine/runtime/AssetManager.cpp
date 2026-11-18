#include "runtime/AssetManager.h"
#include "assets/ModelCooker.h"
#include "assets/PrimitiveMeshes.h"
#include "core/Log.h"
#include "core/StringUtils.h"

#include <algorithm>
#include <thread>

namespace atmos
{
    namespace
    {
        TextureKind GetTextureKind(MaterialTextureSlot slot)
        {
            switch (slot)
            {
                case MaterialTextureSlot::BaseColor:
                case MaterialTextureSlot::Emissive: return TextureKind::Color;
                case MaterialTextureSlot::Normal:   return TextureKind::NormalMap;
                default:                            return TextureKind::Linear;
            }
        }

        uint32_t GetDefaultWorkerCount()
        {
            const uint32_t hardwareThreads = std::max(std::thread::hardware_concurrency(), 1u);
            return std::max(hardwareThreads > 2 ? hardwareThreads - 2 : 1u, 1u);
        }

        // A 1x1 RGBA8 texture uploaded synchronously at startup.
        Texture CreateSolidTexture(GraphicsDevice& device, UploadQueue& uploadQueue, DXGI_FORMAT format,
                                   uint32_t rgba, const wchar_t* name)
        {
            Texture texture(device, { .width = 1, .height = 1, .format = format, .debugName = name });
            const TextureSubresourceData data{ &rgba, 4, 4 };
            uploadQueue.UploadTexture(texture, { &data, 1 });
            return texture;
        }
    }

    AssetManager::AssetManager(GraphicsDevice& device, const Options& options)
        : device_(device),
          cacheDirectory_(options.cacheDirectory),
          uploadQueue_(device),
          workers_(options.workerThreads ? options.workerThreads : GetDefaultWorkerCount())
    {
        std::filesystem::create_directories(cacheDirectory_);
        CreateFallbacks();
    }

    AssetManager::~AssetManager()
    {
        // Workers stop in ~ThreadPool; GPU copies must finish before resources are released.
        try
        {
            uploadQueue_.WaitIdle();
        }
        catch (const std::exception& e)
        {
            log::Error("Failed waiting for uploads: {}", e.what());
        }
    }

    void AssetManager::CreateFallbacks()
    {
        // White works for base color, occlusion, metallic-roughness and emissive (factors scale it);
        // (0.5, 0.5) decodes to a flat tangent-space normal.
        whiteTexture_ = CreateSolidTexture(device_, uploadQueue_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0xFFFFFFFF,
                                           L"Fallback White");
        flatNormalTexture_ = CreateSolidTexture(device_, uploadQueue_, DXGI_FORMAT_R8G8B8A8_UNORM, 0xFFFF8080,
                                                L"Fallback Flat Normal");

        ModelData errorData;
        errorData.meshes.push_back(CreateCubeMeshData());
        errorData.materials.push_back({ .name = "Error", .baseColorFactor = { 1.0f, 0.0f, 1.0f, 1.0f } });
        errorData.instances.push_back({ 0, {} });
        DirectX::XMStoreFloat4x4(&errorData.instances[0].transform, DirectX::XMMatrixIdentity());
        errorModel_ = CreateModel(errorData);

        uploadQueue_.Submit();
        uploadQueue_.WaitIdle();
    }

    ModelHandle AssetManager::LoadModel(const std::filesystem::path& path)
    {
        const std::filesystem::path normalized = std::filesystem::absolute(path).lexically_normal();

        for (uint32_t i = 0; i < models_.size(); ++i)
        {
            if (models_[i].path == normalized)
                return { i };
        }

        const auto index = static_cast<uint32_t>(models_.size());
        models_.push_back({ .path = normalized });

        workers_.Submit([this, index, normalized] {
            CompletedJob job{ .isModel = true, .index = index };

            try
            {
                job.model = LoadCookedModel(normalized, cacheDirectory_);
            }
            catch (const std::exception& e)
            {
                job.error = e.what();
            }

            PushCompleted(std::move(job));
        });

        return { index };
    }

    uint32_t AssetManager::RequestTexture(const std::filesystem::path& path, TextureKind kind)
    {
        for (uint32_t i = 0; i < textures_.size(); ++i)
        {
            if (textures_[i].path == path && textures_[i].kind == kind)
                return i;
        }

        const auto index = static_cast<uint32_t>(textures_.size());
        textures_.push_back({ .path = path, .kind = kind });

        workers_.Submit([this, index, path, kind] {
            CompletedJob job{ .isModel = false, .index = index };

            try
            {
                job.texture = LoadCookedTexture(path, kind, cacheDirectory_);
            }
            catch (const std::exception& e)
            {
                job.error = e.what();
            }

            PushCompleted(std::move(job));
        });

        return index;
    }

    void AssetManager::PushCompleted(CompletedJob job)
    {
        const std::lock_guard lock(completedMutex_);
        completed_.push_back(std::move(job));
    }

    void AssetManager::Update()
    {
        uploadQueue_.ReleaseCompleted();

        std::vector<CompletedJob> completed;
        {
            const std::lock_guard lock(completedMutex_);
            completed.swap(completed_);
        }

        for (CompletedJob& job : completed)
        {
            if (!job.error.empty())
            {
                const std::filesystem::path& path = job.isModel ? models_[job.index].path : textures_[job.index].path;
                log::Error("Failed to load {}: {}", ToUtf8(path.wstring()), job.error);

                if (job.isModel)
                    models_[job.index].state = State::Failed;
                else
                    textures_[job.index].state = State::Failed;

                continue;
            }

            if (job.isModel)
                IntegrateModel(job.index, *job.model);
            else
                IntegrateTexture(job.index, *job.texture);
        }

        // One submission per frame covers everything integrated above.
        const uint64_t fence = uploadQueue_.Submit();

        for (ModelEntry& model : models_)
        {
            if (model.state == State::Uploading && model.uploadFence == 0)
                model.uploadFence = fence;

            if (model.state == State::Uploading && uploadQueue_.IsComplete(model.uploadFence))
            {
                model.state = State::Ready;
                materialsDirty_ = true;
                log::Info("Model ready: {}", ToUtf8(model.path.filename().wstring()));
            }
        }

        for (TextureEntry& texture : textures_)
        {
            if (texture.state == State::Uploading && texture.uploadFence == 0)
                texture.uploadFence = fence;

            if (texture.state == State::Uploading && uploadQueue_.IsComplete(texture.uploadFence))
            {
                texture.state = State::Ready;
                materialsDirty_ = true;
            }
        }

        if (materialsDirty_)
            ResolveMaterialTextures();
    }

    void AssetManager::IntegrateModel(uint32_t index, ModelData& data)
    {
        ModelEntry& entry = models_[index];
        entry.model = CreateModel(data);
        entry.state = State::Uploading;

        // Textures load independently; materials use fallbacks until they are ready.
        entry.materialTextures.resize(data.materials.size());

        for (size_t m = 0; m < data.materials.size(); ++m)
        {
            for (size_t slot = 0; slot < SlotCount; ++slot)
            {
                const std::filesystem::path& texturePath = data.materials[m].textures[slot];
                entry.materialTextures[m][slot] =
                    texturePath.empty() ? NoTexture
                                        : RequestTexture(texturePath, GetTextureKind(static_cast<MaterialTextureSlot>(slot)));
            }
        }
    }

    void AssetManager::IntegrateTexture(uint32_t index, const TextureData& data)
    {
        TextureEntry& entry = textures_[index];
        entry.texture = Texture(device_, {
            .width = data.width,
            .height = data.height,
            .mipLevels = static_cast<uint32_t>(data.mips.size()),
            .format = data.format,
            .debugName = entry.path.filename().wstring(),
        });

        std::vector<TextureSubresourceData> subresources;

        for (const TextureData::Mip& mip : data.mips)
            subresources.push_back({ data.pixels.data() + mip.offset, mip.rowPitch, mip.slicePitch });

        uploadQueue_.UploadTexture(entry.texture, subresources);
        entry.state = State::Uploading;
    }

    LoadedModel AssetManager::CreateModel(const ModelData& data)
    {
        LoadedModel model;
        model.instances = data.instances;

        for (const MeshData& mesh : data.meshes)
        {
            const bool hasGeometry = !mesh.positions.empty() && !mesh.indices.empty();
            model.meshes.push_back(hasGeometry ? std::make_unique<Mesh>(device_, uploadQueue_, mesh) : nullptr);
        }

        model.materials.reserve(data.materials.size());

        for (const MaterialData& materialData : data.materials)
        {
            Material& material = model.materials.emplace_back();
            material.data = materialData;

            for (size_t slot = 0; slot < SlotCount; ++slot)
            {
                const bool isNormal = slot == static_cast<size_t>(MaterialTextureSlot::Normal);
                material.textureIndices[slot] = isNormal ? flatNormalTexture_.GetSrvIndex() : whiteTexture_.GetSrvIndex();
            }
        }

        for (const Material& material : model.materials)
            model.materialPointers.push_back(&material);

        return model;
    }

    void AssetManager::ResolveMaterialTextures()
    {
        materialsDirty_ = false;

        for (ModelEntry& model : models_)
        {
            for (size_t m = 0; m < model.materialTextures.size(); ++m)
            {
                for (size_t slot = 0; slot < SlotCount; ++slot)
                {
                    const uint32_t textureIndex = model.materialTextures[m][slot];

                    if (textureIndex != NoTexture && textures_[textureIndex].state == State::Ready)
                        model.model.materials[m].textureIndices[slot] = textures_[textureIndex].texture.GetSrvIndex();
                }
            }
        }
    }

    const LoadedModel* AssetManager::GetModel(ModelHandle handle) const
    {
        if (!handle.IsValid() || handle.index >= models_.size())
            return nullptr;

        const ModelEntry& entry = models_[handle.index];

        switch (entry.state)
        {
            case State::Ready:  return &entry.model;
            case State::Failed: return &errorModel_;
            default:            return nullptr;
        }
    }

    AssetStats AssetManager::GetStats() const
    {
        AssetStats stats;

        for (const ModelEntry& model : models_)
        {
            stats.modelsReady += model.state == State::Ready;
            stats.modelsFailed += model.state == State::Failed;
            stats.modelsLoading += model.state == State::Loading || model.state == State::Uploading;
        }

        for (const TextureEntry& texture : textures_)
        {
            stats.texturesReady += texture.state == State::Ready;
            stats.texturesFailed += texture.state == State::Failed;
            stats.texturesLoading += texture.state == State::Loading || texture.state == State::Uploading;
        }

        return stats;
    }
}
