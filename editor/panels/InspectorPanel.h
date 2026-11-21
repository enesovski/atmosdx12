#pragma once

#include "panels/Panel.h"

#include "scene/Scene.h"

namespace atmos::editor
{
    // Details of the selection (Unity Inspector / Unreal Details): a header, then one section per
    // component on a property grid. Selecting "Sky & Sun" shows the environment instead.
    class InspectorPanel final : public Panel
    {
    public:
        InspectorPanel();

    protected:
        void DrawContent(EditorContext& context) override;

    private:
        void DrawEnvironment(EditorContext& context);
        void DrawHeader(EditorContext& context, EntityId id);
        void DrawTransform(EditorContext& context, Entity& entity);
        void DrawLight(EditorContext& context, Entity& entity);
        void DrawModel(EditorContext& context, Entity& entity);
        void DrawAddComponent(EditorContext& context, Entity& entity);

        bool uniformScale_ = true;
        float temperatureKelvin_ = 6500.0f;  // last temperature applied to a light color
    };
}
