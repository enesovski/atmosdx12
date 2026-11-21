#pragma once

#include "assets/ModelData.h"
#include "assets/TextureCooker.h"
#include "core/Handle.h"
#include "core/ThreadPool.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/d3d12/Texture.h"
#include "graphics/d3d12/UploadQueue.h"

#include <array>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace atmos
{
    class GraphicsDevice;

    // GPU-resident model, ready to draw.
    struct LoadedModel
    {
        std::vector<std::unique_ptr<Mesh>> meshes;      // nullptr for meshes without geometry
        std::vector<Material> materials;
        std::vector<const Material*> materialPointers;  // what MeshInstance::materials views
        std::vector<ModelInstance> instances;
    };

    struct AssetStats
    {
        uint32_t modelsReady = 0;
        uint32_t modelsLoading = 0;
        uint32_t modelsFailed = 0;
        uint32_t texturesReady = 0;
        uint32_t texturesLoading = 0;
        uint32_t texturesFailed = 0;
    };

    // Loads models and textures behind handles. Workers cook on the CPU; Update() (main thread)
    // creates GPU resources, uploads them on the copy queue and marks them ready.
    class AssetManager
    {
    public:
        struct Options
        {
            std::filesystem::path cacheDirectory;
            uint32_t workerThreads = 0;  // 0: hardware threads minus two
        };

        AssetManager(GraphicsDevice& device, const Options& options);
        ~AssetManager();

        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

        // Asynchronous; deduplicated by path.
        ModelHandle LoadModel(const std::filesystem::path& path);

        void Update();

        // Ready model, the error model if loading failed, or nullptr while loading.
        const LoadedModel* GetModel(ModelHandle handle) const;

        AssetStats GetStats() const;

    private:
        enum class State
        {
            Loading,    // CPU job queued or running
            Uploading,  // GPU resources created, copies in flight
            Ready,
            Failed,
        };

        static constexpr size_t SlotCount = static_cast<size_t>(MaterialTextureSlot::Count);
        static constexpr uint32_t NoTexture = UINT32_MAX;

        struct TextureEntry
        {
            std::filesystem::path path;
            TextureKind kind;
            State state = State::Loading;
            Texture texture;
            uint64_t uploadFence = 0;
        };

        struct ModelEntry
        {
            std::filesystem::path path;
            State state = State::Loading;
            LoadedModel model;
            std::vector<std::array<uint32_t, SlotCount>> materialTextures;  // TextureEntry index per slot
            uint64_t uploadFence = 0;
        };

        // Written by worker threads, consumed by Update().
        struct CompletedJob
        {
            bool isModel = false;
            uint32_t index = 0;
            std::optional<ModelData> model;
            std::optional<TextureData> texture;
            std::string error;
        };

        void CreateFallbacks();
        uint32_t RequestTexture(const std::filesystem::path& path, TextureKind kind);
        void IntegrateModel(uint32_t index, ModelData& data);
        void IntegrateTexture(uint32_t index, const TextureData& data);
        void ResolveMaterialTextures();
        LoadedModel CreateModel(const ModelData& data);
        void PushCompleted(CompletedJob job);

        GraphicsDevice& device_;
        std::filesystem::path cacheDirectory_;
        UploadQueue uploadQueue_;

        std::vector<ModelEntry> models_;
        std::vector<TextureEntry> textures_;

        Texture whiteTexture_;
        Texture flatNormalTexture_;
        LoadedModel errorModel_;
        bool materialsDirty_ = false;

        std::mutex completedMutex_;
        std::vector<CompletedJob> completed_;

        // Last member: workers are joined before the state they write is destroyed.
        ThreadPool workers_;
    };
}
