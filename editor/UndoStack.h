#pragma once

#include "EditorSelection.h"

#include "scene/Scene.h"

#include <cstddef>
#include <deque>
#include <optional>
#include <string>

namespace atmos::editor
{
    // Snapshot undo: every committed edit stores a copy of the whole scene. Scenes are small value
    // types, so this is trivially correct for every kind of edit (property, create, delete,
    // reparent) and needs no per-operation undo code.
    class UndoStack
    {
    public:
        static constexpr size_t MaxSteps = 256;

        // Starts a new history whose only state is clean (just loaded or saved).
        void Reset(const Scene& scene, const EditorSelection& selection);

        // Records the state after an edit; drops any redo states.
        void Push(std::string label, const Scene& scene, const EditorSelection& selection);

        bool CanUndo() const { return current_ > 0; }
        bool CanRedo() const { return current_ + 1 < states_.size(); }
        const std::string& GetUndoLabel() const;
        const std::string& GetRedoLabel() const;

        bool Undo(Scene& scene, EditorSelection& selection);
        bool Redo(Scene& scene, EditorSelection& selection);

        void MarkClean() { clean_ = current_; }
        bool IsClean() const { return clean_ == current_; }

    private:
        struct State
        {
            std::string label;  // the edit that produced this state
            Scene scene;
            EditorSelection selection;
        };

        void Restore(Scene& scene, EditorSelection& selection) const;

        std::deque<State> states_;
        size_t current_ = 0;
        std::optional<size_t> clean_;  // empty once the clean state was trimmed away
    };
}
