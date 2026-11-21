#include "EditorSelection.h"

#include <algorithm>

namespace atmos::editor
{
    void EditorSelection::Clear()
    {
        entities_.clear();
        environment_ = false;
    }

    void EditorSelection::Select(EntityId id)
    {
        Clear();
        entities_.push_back(id);
    }

    void EditorSelection::Add(EntityId id)
    {
        environment_ = false;
        std::erase(entities_, id);
        entities_.push_back(id);
    }

    void EditorSelection::Toggle(EntityId id)
    {
        environment_ = false;

        if (IsSelected(id))
            std::erase(entities_, id);
        else
            entities_.push_back(id);
    }

    void EditorSelection::SelectEnvironment()
    {
        entities_.clear();
        environment_ = true;
    }

    bool EditorSelection::IsSelected(EntityId id) const
    {
        return std::ranges::find(entities_, id) != entities_.end();
    }

    std::vector<uint32_t> EditorSelection::GetSortedEntities() const
    {
        std::vector<uint32_t> sorted(entities_.begin(), entities_.end());
        std::ranges::sort(sorted);
        return sorted;
    }

    void EditorSelection::Remap(std::span<const EntityId> remap)
    {
        for (EntityId& id : entities_)
            id = id < remap.size() ? remap[id] : InvalidEntity;

        std::erase(entities_, InvalidEntity);
    }

    void EditorSelection::Validate(const Scene& scene)
    {
        std::erase_if(entities_, [&](EntityId id) { return !scene.IsValid(id); });
    }
}
