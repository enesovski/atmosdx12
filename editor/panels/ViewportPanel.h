#pragma once

#include "panels/Panel.h"

#include "graphics/d3d12/Texture.h"
#include "scene/Scene.h"

#include <DirectXMath.h>

#include <optional>
#include <vector>

namespace atmos
{
    class GraphicsDevice;
}

namespace atmos::editor
{
    class ViewportProjection;

    // The scene view: the renderer draws into an offscreen texture at this panel's size, shown as an
    // image with editor overlays on top (toolbar, stats, light icons, gizmo). Clicking selects,
    // models dropped from the content browser are placed where they land.
    class ViewportPanel final : public Panel
    {
    public:
        explicit ViewportPanel(GraphicsDevice& device);

        // The texture to render into this frame; null while the panel is hidden or has no size.
        Texture* GetTarget();

        // Pointer over the scene image itself (not a toolbar button or the gizmo); updated each frame.
        bool IsHovered() const { return imageHovered_; }
        bool IsFocused() const { return focused_; }

    protected:
        void DrawContent(EditorContext& context) override;
        ImGuiWindowFlags GetWindowFlags() const override;
        bool HasPadding() const override { return false; }

    private:
        struct LightIcon
        {
            EntityId entity;
            DirectX::XMFLOAT2 screen;
        };

        void UpdateTarget(EditorContext& context, ImVec2 size);
        void DrawToolbar(EditorContext& context);
        void DrawViewModeMenu(EditorContext& context);
        void DrawShowMenu(EditorContext& context);
        void DrawCameraMenu(EditorContext& context);
        void DrawStats(EditorContext& context);
        void DrawLights(EditorContext& context, const ViewportProjection& projection);
        void HandleClick(EditorContext& context, const ViewportProjection& projection, bool gizmoActive);
        void HandleDrop(EditorContext& context, const ViewportProjection& projection);
        void HandleBookmarks(EditorContext& context);

        GraphicsDevice& device_;
        Texture target_;
        ImVec2 imageMin_{};
        ImVec2 imageSize_{};
        bool imageHovered_ = false;
        bool focused_ = false;
        bool clickStarted_ = false;
        ImVec2 clickPosition_{};
        std::optional<EntityId> clickedLight_;
        std::vector<LightIcon> lightIcons_;
        std::optional<EntityId> hoveredLight_;
    };
}
