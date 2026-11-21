#pragma once

#include "panels/Panel.h"

namespace atmos::editor
{
    // The scene's render settings (Unity's Lighting window / an Unreal post-process volume): a quality
    // preset on top, then one collapsible category per feature. Values are saved with the scene and
    // every change is undoable. Later phases add their categories here (TAA, AO, reflections, GI...).
    class RenderSettingsPanel final : public Panel
    {
    public:
        RenderSettingsPanel();

    protected:
        void DrawContent(EditorContext& context) override;
    };
}
