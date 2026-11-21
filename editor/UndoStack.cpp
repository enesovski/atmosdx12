#include "UndoStack.h"

namespace atmos::editor
{
    namespace
    {
        const std::string EmptyLabel;
    }

    void UndoStack::Reset(const Scene& scene, const EditorSelection& selection)
    {
        states_.clear();
        states_.push_back({ {}, scene, selection });
        current_ = 0;
        clean_ = 0;
    }

    void UndoStack::Push(std::string label, const Scene& scene, const EditorSelection& selection)
    {
        states_.resize(current_ + 1);

        if (clean_ && *clean_ > current_)
            clean_.reset();

        states_.push_back({ std::move(label), scene, selection });
        ++current_;

        if (states_.size() > MaxSteps + 1)
        {
            states_.pop_front();
            --current_;

            if (clean_)
                clean_ = *clean_ == 0 ? std::nullopt : std::optional<size_t>(*clean_ - 1);
        }
    }

    const std::string& UndoStack::GetUndoLabel() const
    {
        return CanUndo() ? states_[current_].label : EmptyLabel;
    }

    const std::string& UndoStack::GetRedoLabel() const
    {
        return CanRedo() ? states_[current_ + 1].label : EmptyLabel;
    }

    bool UndoStack::Undo(Scene& scene, EditorSelection& selection)
    {
        if (!CanUndo())
            return false;

        --current_;
        Restore(scene, selection);
        return true;
    }

    bool UndoStack::Redo(Scene& scene, EditorSelection& selection)
    {
        if (!CanRedo())
            return false;

        ++current_;
        Restore(scene, selection);
        return true;
    }

    void UndoStack::Restore(Scene& scene, EditorSelection& selection) const
    {
        scene = states_[current_].scene;
        selection = states_[current_].selection;
    }
}
