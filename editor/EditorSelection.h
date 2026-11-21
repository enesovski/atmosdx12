#pragma once

#include "scene/Scene.h"

#include <span>
#include <vector>

namespace atmos::editor
{
    // Selected entities, primary last; or the Sky & Sun pseudo-entity (never both).
    class EditorSelection
    {
    public:
        void Clear();
        void Select(EntityId id);
        void Add(EntityId id);
        void Toggle(EntityId id);
        void SelectEnvironment();

        bool IsSelected(EntityId id) const;
        bool IsEnvironmentSelected() const { return environment_; }
        bool HasEntities() const { return !entities_.empty(); }
        EntityId GetPrimary() const { return entities_.empty() ? InvalidEntity : entities_.back(); }
        std::span<const EntityId> GetEntities() const { return entities_; }
        std::vector<uint32_t> GetSortedEntities() const;

        // Follows ids after a structural scene edit (old id -> new id, InvalidEntity if removed).
        void Remap(std::span<const EntityId> remap);

        // Drops ids that no longer exist, e.g. after loading another scene.
        void Validate(const Scene& scene);

    private:
        std::vector<EntityId> entities_;
        bool environment_ = false;
    };
}
