#include "SceneDocument.h"

#include "core/Log.h"
#include "core/StringUtils.h"
#include "runtime/AssetManager.h"
#include "runtime/RenderSceneBuilder.h"
#include "scene/SceneSerializer.h"

#include <format>
#include <random>

namespace atmos::editor
{
    SceneDocument::SceneDocument(AssetManager& assets) : assets_(assets)
    {
        ResetHistory();
    }

    void SceneDocument::New()
    {
        scene_ = {};
        scene_.environment.sunIntensity = 8.0f;
        path_.clear();
        selection_.Clear();
        ResetHistory();
    }

    bool SceneDocument::Open(const std::filesystem::path& path)
    {
        try
        {
            const std::filesystem::path extension = path.extension();
            Scene scene;

            if (extension == ".gltf" || extension == ".glb")
            {
                // A model on its own becomes an untitled scene with one entity.
                scene.environment.sunIntensity = 8.0f;
                const EntityId entity = scene.CreateEntity(ToUtf8(path.stem().wstring()));
                scene.GetEntity(entity).model = ModelComponent{ .path = std::filesystem::absolute(path) };
                path_.clear();
            }
            else
            {
                scene = LoadScene(path);
                path_ = std::filesystem::absolute(path);
            }

            scene_ = std::move(scene);
            selection_.Clear();
            RequestSceneAssets(scene_, assets_);
            ResetHistory();
            log::Info("Opened {} ({} entities)", ToUtf8(path.filename().wstring()), scene_.GetEntityCount());
            return true;
        }
        catch (const std::exception& e)
        {
            log::Error("Failed to open {}: {}", ToUtf8(path.wstring()), e.what());
            return false;
        }
    }

    bool SceneDocument::Save()
    {
        return !path_.empty() && SaveAs(path_);
    }

    bool SceneDocument::SaveAs(const std::filesystem::path& path)
    {
        CommitPendingEdit();

        try
        {
            SaveScene(scene_, path);
            path_ = std::filesystem::absolute(path);
            undo_.MarkClean();
            log::Info("Saved {}", ToUtf8(path_.wstring()));
            return true;
        }
        catch (const std::exception& e)
        {
            log::Error("Failed to save {}: {}", ToUtf8(path.wstring()), e.what());
            return false;
        }
    }

    std::string SceneDocument::GetDisplayName() const
    {
        return path_.empty() ? std::string("Untitled") : ToUtf8(path_.stem().wstring());
    }

    void SceneDocument::MarkEdited(std::string label)
    {
        pendingEdit_ = std::move(label);
    }

    void SceneDocument::CommitPendingEdit()
    {
        if (!pendingEdit_)
            return;

        undo_.Push(std::move(*pendingEdit_), scene_, selection_);
        pendingEdit_.reset();
    }

    void SceneDocument::Undo()
    {
        CommitPendingEdit();

        if (undo_.Undo(scene_, selection_))
            RequestSceneAssets(scene_, assets_);
    }

    void SceneDocument::Redo()
    {
        CommitPendingEdit();

        if (undo_.Redo(scene_, selection_))
            RequestSceneAssets(scene_, assets_);
    }

    EntityId SceneDocument::CreateEntity(const std::string& name, EntityId parent)
    {
        CommitPendingEdit();
        const EntityId id = scene_.CreateEntity(MakeUniqueName(name), parent);
        selection_.Select(id);
        MarkEdited("Create " + name);
        return id;
    }

    EntityId SceneDocument::CreatePointLight(const DirectX::XMFLOAT3& position)
    {
        const EntityId id = CreateEntity("Point Light");
        scene_.GetEntity(id).transform.position = position;
        scene_.GetEntity(id).light = LightComponent{ .intensity = 10.0f, .range = 8.0f };
        return id;
    }

    EntityId SceneDocument::CreateSpotLight(const DirectX::XMFLOAT3& position)
    {
        const EntityId id = CreateEntity("Spot Light");
        Entity& entity = scene_.GetEntity(id);
        entity.transform.position = position;
        entity.transform.rotationDegrees = { 90.0f, 0.0f, 0.0f };  // pointing down
        entity.light = LightComponent{ .type = LightType::Spot, .intensity = 40.0f, .range = 12.0f, .castsShadows = true };
        return id;
    }

    EntityId SceneDocument::AddModel(const std::filesystem::path& path, const DirectX::XMFLOAT3& position)
    {
        const EntityId id = CreateEntity(ToUtf8(path.stem().wstring()));
        Entity& entity = scene_.GetEntity(id);
        entity.transform.position = position;
        entity.model = ModelComponent{ .path = std::filesystem::absolute(path) };
        RequestSceneAssets(scene_, assets_);
        return id;
    }

    void SceneDocument::DeleteSelection()
    {
        if (!selection_.HasEntities())
            return;

        CommitPendingEdit();
        const size_t count = selection_.GetEntities().size();
        const std::vector<EntityId> ids(selection_.GetEntities().begin(), selection_.GetEntities().end());
        scene_.DestroyEntities(ids);
        selection_.Clear();
        MarkEdited(count == 1 ? "Delete Entity" : std::format("Delete {} Entities", count));
    }

    void SceneDocument::DuplicateSelection()
    {
        if (!selection_.HasEntities())
            return;

        CommitPendingEdit();
        std::vector<EntityId> pending(selection_.GetEntities().begin(), selection_.GetEntities().end());
        std::vector<EntityId> copies;

        // Each duplicate renumbers the scene, so earlier results and pending ids are remapped.
        for (size_t i = 0; i < pending.size(); ++i)
        {
            std::vector<EntityId> remap;
            const EntityId copy = scene_.DuplicateEntity(pending[i], remap);

            for (EntityId& id : pending)
                id = remap[id];

            for (EntityId& id : copies)
                id = remap[id];

            copies.push_back(copy);
        }

        selection_.Clear();

        for (EntityId copy : copies)
            selection_.Add(copy);

        MarkEdited("Duplicate");
    }

    void SceneDocument::Reparent(EntityId id, EntityId parent)
    {
        CommitPendingEdit();
        const std::vector<EntityId> remap = scene_.SetParent(id, parent);

        if (remap.empty())
            return;

        selection_.Remap(remap);
        MarkEdited("Reparent");
    }

    void SceneDocument::Rename(EntityId id, std::string name)
    {
        if (!scene_.IsValid(id) || name.empty() || scene_.GetEntity(id).name == name)
            return;

        scene_.GetEntity(id).name = std::move(name);
        MarkEdited("Rename");
    }

    // Scatters colored point lights through the Sponza atrium as one undo step. Seeded by the entity
    // count, so a run is repeatable.
    void SceneDocument::SpawnTestLights(uint32_t count)
    {
        if (count == 0)
            return;

        CommitPendingEdit();
        std::mt19937 random(static_cast<uint32_t>(scene_.GetEntityCount()));
        std::uniform_real_distribution<float> x(-13.0f, 12.0f), y(0.5f, 8.0f), z(-5.5f, 5.5f);
        std::uniform_real_distribution<float> hue(0.0f, 1.0f), range(5.0f, 8.0f);

        for (uint32_t i = 0; i < count; ++i)
        {
            const EntityId id = scene_.CreateEntity(std::format("Test Light {}", i));
            Entity& entity = scene_.GetEntity(id);
            entity.transform.position = { x(random), y(random), z(random) };

            DirectX::XMFLOAT3 color;
            DirectX::XMStoreFloat3(&color, DirectX::XMColorHSVToRGB(DirectX::XMVectorSet(hue(random), 0.7f, 1.0f, 0.0f)));
            entity.light = LightComponent{ .color = color, .intensity = 4.0f, .range = range(random) };
        }

        MarkEdited(std::format("Spawn {} Lights", count));
        log::Info("Spawned {} test lights", count);
    }

    void SceneDocument::ResetHistory()
    {
        pendingEdit_.reset();
        undo_.Reset(scene_, selection_);
    }

    std::string SceneDocument::MakeUniqueName(const std::string& base) const
    {
        auto exists = [&](const std::string& name) {
            for (const Entity& entity : scene_.GetEntities())
            {
                if (entity.name == name)
                    return true;
            }

            return false;
        };

        if (!exists(base))
            return base;

        for (int suffix = 1;; ++suffix)
        {
            std::string candidate = std::format("{} {}", base, suffix);

            if (!exists(candidate))
                return candidate;
        }
    }
}
