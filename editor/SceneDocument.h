#pragma once

#include "EditorSelection.h"
#include "UndoStack.h"

#include "scene/Scene.h"

#include <DirectXMath.h>

#include <filesystem>
#include <optional>
#include <string>

namespace atmos
{
    class AssetManager;
}

namespace atmos::editor
{
    // The open scene and everything that edits it: file, selection, undo history.
    // Panels change the scene directly for live feedback, then call MarkEdited; the edit becomes one
    // undo step at the end of the frame once no widget or gizmo is still being dragged.
    class SceneDocument
    {
    public:
        explicit SceneDocument(AssetManager& assets);

        // File operations. Open and Save log and return false on failure.
        void New();
        bool Open(const std::filesystem::path& path);
        bool Save();
        bool SaveAs(const std::filesystem::path& path);

        Scene& GetScene() { return scene_; }
        const Scene& GetScene() const { return scene_; }
        const std::filesystem::path& GetPath() const { return path_; }
        std::string GetDisplayName() const;
        bool IsDirty() const { return !undo_.IsClean() || pendingEdit_.has_value(); }

        EditorSelection& GetSelection() { return selection_; }
        const EditorSelection& GetSelection() const { return selection_; }

        // Undo
        void MarkEdited(std::string label);
        void CommitPendingEdit();
        void Undo();
        void Redo();
        const UndoStack& GetUndoStack() const { return undo_; }

        // Structural edits; each selects its result and is one undo step.
        EntityId CreateEntity(const std::string& name, EntityId parent = InvalidEntity);
        EntityId CreatePointLight(const DirectX::XMFLOAT3& position);
        EntityId CreateSpotLight(const DirectX::XMFLOAT3& position);
        EntityId AddModel(const std::filesystem::path& path, const DirectX::XMFLOAT3& position);
        void DeleteSelection();
        void DuplicateSelection();
        void Reparent(EntityId id, EntityId parent);
        void Rename(EntityId id, std::string name);

        // Random colored point lights inside Sponza's atrium, for stress tests; one undo step.
        void SpawnTestLights(uint32_t count);

    private:
        void ResetHistory();
        std::string MakeUniqueName(const std::string& base) const;

        AssetManager& assets_;
        Scene scene_;
        std::filesystem::path path_;  // empty for an unsaved scene
        EditorSelection selection_;
        UndoStack undo_;
        std::optional<std::string> pendingEdit_;
    };
}
