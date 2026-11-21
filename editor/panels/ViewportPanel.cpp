#include "panels/ViewportPanel.h"
#include "EditorContext.h"
#include "ViewModes.h"
#include "style/EditorFonts.h"
#include "style/EditorTheme.h"
#include "tools/Gizmo.h"
#include "tools/Picking.h"
#include "widgets/Widgets.h"

#include "core/StringUtils.h"
#include "graphics/Renderer.h"
#include "graphics/d3d12/GraphicsConfig.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

using namespace DirectX;

namespace atmos::editor
{
    namespace
    {
        constexpr float IconPickRadius = 14.0f;  // pixels at 100% scale
        constexpr int CircleSegments = 48;
        constexpr float ClickDragThreshold = 4.0f;

        ImVec2 ToImVec2(const XMFLOAT2& value)
        {
            return ImVec2(value.x, value.y);
        }

        // Projects a world-space polyline, clipping segments that cross behind the camera.
        void DrawWorldPolyline(ImDrawList* draw, const ViewportProjection& projection, const XMFLOAT3* points, int count,
                               bool closed, ImU32 color, float thickness)
        {
            const int segments = closed ? count : count - 1;

            for (int i = 0; i < segments; ++i)
            {
                XMFLOAT2 a, b;

                if (projection.ProjectSegment(points[i], points[(i + 1) % count], a, b))
                    draw->AddLine(ToImVec2(a), ToImVec2(b), color, thickness);
            }
        }

        void DrawCircle(ImDrawList* draw, const ViewportProjection& projection, FXMVECTOR center, FXMVECTOR axisA,
                        FXMVECTOR axisB, float radius, ImU32 color, float thickness)
        {
            XMFLOAT3 points[CircleSegments];

            for (int i = 0; i < CircleSegments; ++i)
            {
                const float angle = XM_2PI * i / CircleSegments;
                XMStoreFloat3(&points[i], center + (axisA * std::cos(angle) + axisB * std::sin(angle)) * radius);
            }

            DrawWorldPolyline(draw, projection, points, CircleSegments, true, color, thickness);
        }

        void DrawLightRange(ImDrawList* draw, const ViewportProjection& projection, const LightComponent& light,
                            const XMFLOAT4X4& world, ImU32 color, float thickness)
        {
            const XMVECTOR position = XMVectorSet(world._41, world._42, world._43, 1.0f);

            if (light.type == LightType::Point)
            {
                const XMVECTOR x = XMVectorSet(1, 0, 0, 0), y = XMVectorSet(0, 1, 0, 0), z = XMVectorSet(0, 0, 1, 0);
                DrawCircle(draw, projection, position, x, y, light.range, color, thickness);
                DrawCircle(draw, projection, position, x, z, light.range, color, thickness);
                DrawCircle(draw, projection, position, y, z, light.range, color, thickness);
                return;
            }

            // Spot: the outer cone out to the range, with its end cap.
            const XMVECTOR direction = XMVector3Normalize(XMVectorSet(world._31, world._32, world._33, 0.0f));
            const XMVECTOR helper = std::abs(XMVectorGetY(direction)) > 0.99f ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 1, 0, 0);
            const XMVECTOR side = XMVector3Normalize(XMVector3Cross(helper, direction));
            const XMVECTOR up = XMVector3Cross(direction, side);

            const float angle = XMConvertToRadians(std::clamp(light.outerConeDegrees, 1.0f, 89.0f));
            const XMVECTOR capCenter = position + direction * (light.range * std::cos(angle));
            const float capRadius = light.range * std::sin(angle);
            DrawCircle(draw, projection, capCenter, side, up, capRadius, color, thickness);

            const XMVECTOR edges[4] = { side, -side, up, -up };

            for (const XMVECTOR& edge : edges)
            {
                XMFLOAT3 line[2];
                XMStoreFloat3(&line[0], position);
                XMStoreFloat3(&line[1], capCenter + edge * capRadius);
                DrawWorldPolyline(draw, projection, line, 2, false, color, thickness);
            }
        }

        ImU32 GetIconColor(const XMFLOAT3& color)
        {
            // Normalized hue, lifted toward white so dim or saturated lights stay readable.
            const float peak = std::max({ color.x, color.y, color.z, 1e-3f });
            return ImGui::ColorConvertFloat4ToU32(ImVec4(0.35f + 0.65f * color.x / peak, 0.35f + 0.65f * color.y / peak,
                                                         0.35f + 0.65f * color.z / peak, 1.0f));
        }
    }

    ViewportPanel::ViewportPanel(GraphicsDevice& device) : Panel("Viewport", Icon::Viewport), device_(device)
    {
    }

    Texture* ViewportPanel::GetTarget()
    {
        return IsOpen() && IsVisible() && target_.IsValid() ? &target_ : nullptr;
    }

    ImGuiWindowFlags ViewportPanel::GetWindowFlags() const
    {
        return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;
    }

    void ViewportPanel::DrawContent(EditorContext& context)
    {
        focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        imageMin_ = ImGui::GetCursorScreenPos();
        imageSize_ = ImGui::GetContentRegionAvail();
        UpdateTarget(context, imageSize_);

        const ImVec2 imageMax(imageMin_.x + imageSize_.x, imageMin_.y + imageSize_.y);
        ImDrawList* draw = ImGui::GetWindowDrawList();

        if (target_.IsValid())
            draw->AddImage(static_cast<ImTextureID>(target_.GetSrvGpuHandle().ptr), imageMin_, imageMax);

        const ViewportProjection projection(context.camera, { imageMin_.x, imageMin_.y }, { imageSize_.x, imageSize_.y });

        draw->PushClipRect(imageMin_, imageMax, true);
        DrawLights(context, projection);
        const bool gizmoActive = DrawTransformGizmo(context, { imageMin_.x, imageMin_.y }, { imageSize_.x, imageSize_.y });
        draw->PopClipRect();

        DrawToolbar(context);
        DrawStats(context);

        // Hover is decided last, so toolbar buttons and the gizmo take precedence over the image.
        imageHovered_ = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(imageMin_, imageMax) &&
                        !ImGui::IsAnyItemHovered() && !gizmoActive;

        HandleClick(context, projection, gizmoActive);
        HandleDrop(context, projection);
        HandleBookmarks(context);
    }

    void ViewportPanel::UpdateTarget(EditorContext& context, ImVec2 size)
    {
        if (size.x < 1.0f || size.y < 1.0f)
            return;

        const float scale = context.viewport.resolutionScale;
        const auto width = static_cast<uint32_t>(std::max(1.0f, std::round(size.x * scale)));
        const auto height = static_cast<uint32_t>(std::max(1.0f, std::round(size.y * scale)));

        if (target_.IsValid() && target_.GetWidth() == width && target_.GetHeight() == height)
            return;

        // The old texture is released through the deferred-release queue once the GPU is done with it.
        target_ = Texture(device_, {
            .width = width,
            .height = height,
            .format = GraphicsConfig::BackBufferFormat,
            .usage = TextureUsage::ShaderResource | TextureUsage::RenderTarget,
            .debugName = L"Viewport",
        });
    }

    void ViewportPanel::DrawToolbar(EditorContext& context)
    {
        const ThemeColors& colors = GetThemeColors();
        const float padding = ImGui::GetStyle().WindowPadding.x;

        ImGui::SetCursorScreenPos(ImVec2(imageMin_.x + padding, imageMin_.y + padding));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(colors.panel.x, colors.panel.y, colors.panel.z, 0.82f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors.panelRaised);

        widgets::DropdownButton(GetViewMode(context.viewport.viewMode).name, Icon::Image, "ViewModeMenu", "View mode (Alt+1..9)");
        DrawViewModeMenu(context);

        ImGui::SameLine();
        widgets::DropdownButton("Show", Icon::Eye, "ShowMenu", "Show flags");
        DrawShowMenu(context);

        ImGui::SameLine();
        const std::string speed = std::format("{:.1f} m/s", context.cameraController.GetMoveSpeed());
        widgets::DropdownButton(speed.c_str(), Icon::Camera, "CameraMenu", "Camera settings and bookmarks");
        DrawCameraMenu(context);

        ImGui::SameLine();
        const std::string scale = std::format("{:.0f}%", context.viewport.resolutionScale * 100.0f);
        widgets::DropdownButton(scale.c_str(), Icon::Grid, "ResolutionMenu", "Resolution scale");

        if (ImGui::BeginPopup("ResolutionMenu"))
        {
            for (const float option : { 0.5f, 0.67f, 0.75f, 1.0f })
            {
                const std::string label = std::format("{:.0f}%", option * 100.0f);

                if (ImGui::MenuItem(label.c_str(), nullptr, std::abs(context.viewport.resolutionScale - option) < 0.01f))
                    context.viewport.resolutionScale = option;
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(2);
    }

    void ViewportPanel::DrawViewModeMenu(EditorContext& context)
    {
        if (!ImGui::BeginPopup("ViewModeMenu"))
            return;

        const char* group = nullptr;
        int index = 0;

        for (const ViewMode& mode : GetViewModes())
        {
            if (!group || std::string_view(group) != mode.group)
            {
                group = mode.group;
                ImGui::SeparatorText(group);
            }

            const std::string shortcut = ++index <= 9 ? std::format("Alt+{}", index) : std::string();

            if (ImGui::MenuItem(mode.name, shortcut.c_str(), context.viewport.viewMode == mode.debugView))
                context.viewport.viewMode = mode.debugView;
        }

        ImGui::EndPopup();
    }

    void ViewportPanel::DrawShowMenu(EditorContext& context)
    {
        if (!ImGui::BeginPopup("ShowMenu"))
            return;

        ViewportSettings& viewport = context.viewport;
        ImGui::SeparatorText("Overlays");
        ImGui::MenuItem("Light Icons", nullptr, &viewport.showLightIcons);
        ImGui::MenuItem("Light Ranges (selected)", nullptr, &viewport.showLightRanges);
        ImGui::MenuItem("Selection Outline", nullptr, &viewport.showSelectionOutline);
        ImGui::MenuItem("Wireframe", nullptr, &viewport.wireframe);
        ImGui::SeparatorText("Information");
        ImGui::MenuItem("Stats", nullptr, &viewport.showStats);
        ImGui::EndPopup();
    }

    void ViewportPanel::DrawCameraMenu(EditorContext& context)
    {
        if (!ImGui::BeginPopup("CameraMenu"))
            return;

        Camera& camera = context.camera;
        ImGui::SeparatorText("Camera");
        ImGui::PushItemWidth(ImGui::GetFontSize() * 12.0f);

        float fov = camera.GetFovDegrees();
        if (ImGui::SliderFloat("Field of view", &fov, 35.0f, 95.0f, "%.0f deg"))
            camera.SetFov(fov);

        float speed = context.cameraController.GetMoveSpeed();
        if (ImGui::SliderFloat("Move speed", &speed, 0.25f, 200.0f, "%.1f m/s", ImGuiSliderFlags_Logarithmic))
            context.cameraController.SetMoveSpeed(speed);

        float nearPlane = camera.GetNearPlane();
        if (ImGui::SliderFloat("Near plane", &nearPlane, 0.01f, 1.0f, "%.2f m", ImGuiSliderFlags_Logarithmic))
            camera.SetNearPlane(nearPlane);

        ImGui::PopItemWidth();
        ImGui::SeparatorText("Bookmarks (Ctrl+N saves, N recalls)");
        Scene& scene = context.document.GetScene();

        for (size_t slot = 0; slot < Scene::BookmarkCount; ++slot)
        {
            ImGui::PushID(static_cast<int>(slot));
            const std::optional<SceneCamera>& bookmark = scene.cameraBookmarks[slot];
            const std::string label = std::format("{}  Bookmark {}", std::string(Icon::Bookmark), slot + 1);

            if (ImGui::MenuItem(label.c_str(), std::to_string(slot + 1).c_str(), false, bookmark.has_value()))
            {
                camera.SetPosition(bookmark->position);
                camera.SetRotation(bookmark->yawDegrees, bookmark->pitchDegrees);
                camera.SetFov(bookmark->fovDegrees);
            }

            ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());

            if (widgets::IconButton("save", Icon::Save, "Save the current view here"))
            {
                scene.cameraBookmarks[slot] = SceneCamera{ camera.GetPosition(), camera.GetYawDegrees(),
                                                           camera.GetPitchDegrees(), camera.GetFovDegrees() };
                context.document.MarkEdited("Set Camera Bookmark");
            }

            ImGui::PopID();
        }

        ImGui::EndPopup();
    }

    void ViewportPanel::DrawStats(EditorContext& context)
    {
        if (!context.viewport.showStats)
            return;

        const RenderFrameStats& stats = context.renderer.GetFrameStats();
        const float cpuMs = context.deltaSeconds * 1000.0f;
        const std::string lines[] = {
            std::format("{:5.0f} FPS", cpuMs > 0.0f ? 1000.0f / cpuMs : 0.0f),
            std::format("CPU {:6.2f} ms", cpuMs),
            std::format("GPU {:6.2f} ms", stats.gpuMilliseconds),
            std::format("{} x {}", stats.renderWidth, stats.renderHeight),
            std::format("{} draws, {} lights", stats.scene.visibleDraws, stats.scene.lights),
        };

        ImFont* font = GetFont(FontRole::Mono);
        const float lineHeight = font->FontSize;
        float width = 0.0f;

        for (const std::string& line : lines)
            width = std::max(width, font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, line.c_str()).x);

        const float padding = ImGui::GetStyle().WindowPadding.x;
        const ImVec2 max(imageMin_.x + imageSize_.x - padding, imageMin_.y + padding + lineHeight * std::size(lines) + padding);
        const ImVec2 min(max.x - width - 2.0f * padding, imageMin_.y + padding);

        const ThemeColors& colors = GetThemeColors();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(min, max, ToU32(colors.panel, 0.82f), ImGui::GetStyle().WindowRounding);

        for (size_t i = 0; i < std::size(lines); ++i)
        {
            draw->AddText(font, font->FontSize, ImVec2(min.x + padding, min.y + padding * 0.5f + lineHeight * i),
                          ToU32(i == 0 ? colors.text : colors.textDim), lines[i].c_str());
        }
    }

    void ViewportPanel::DrawLights(EditorContext& context, const ViewportProjection& projection)
    {
        lightIcons_.clear();
        hoveredLight_.reset();

        const Scene& scene = context.document.GetScene();
        const EditorSelection& selection = context.document.GetSelection();
        const std::vector<XMFLOAT4X4> world = scene.ComputeWorldMatrices();
        const std::vector<bool> active = scene.ComputeActiveInHierarchy();
        const ThemeColors& colors = GetThemeColors();
        const float uiScale = ImGui::GetFontSize() / 15.0f;
        ImDrawList* draw = ImGui::GetWindowDrawList();

        for (EntityId id = 0; id < scene.GetEntityCount(); ++id)
        {
            const Entity& entity = scene.GetEntity(id);

            if (!entity.light)
                continue;

            const bool selected = selection.IsSelected(id);

            if (selected && context.viewport.showLightRanges)
                DrawLightRange(draw, projection, *entity.light, world[id], ToU32(colors.warning, 0.75f), 1.5f * uiScale);

            if (!context.viewport.showLightIcons)
                continue;

            const XMFLOAT3 position(world[id]._41, world[id]._42, world[id]._43);
            const std::optional<XMFLOAT2> screen = projection.Project(position);

            if (!screen)
                continue;

            lightIcons_.push_back({ id, *screen });

            ImFont* font = GetFont(FontRole::Icons);
            const IconGlyph& icon = entity.light->type == LightType::Spot ? Icon::SpotLight : Icon::PointLight;
            const ImVec2 iconSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, icon);
            const ImVec2 center = ToImVec2(*screen);
            const float radius = font->FontSize * 0.75f;
            const float alpha = active[id] ? 1.0f : 0.4f;

            draw->AddCircleFilled(center, radius, ToU32(colors.panel, 0.65f * alpha));

            if (selected)
                draw->AddCircle(center, radius, ToU32(colors.accent), 0, 2.0f * uiScale);

            const ImU32 iconColor = (GetIconColor(entity.light->color) & 0x00FFFFFF) |
                                    (static_cast<ImU32>(255 * alpha) << IM_COL32_A_SHIFT);
            draw->AddText(font, font->FontSize, ImVec2(center.x - iconSize.x * 0.5f, center.y - iconSize.y * 0.5f),
                          iconColor, icon);
        }

        // The icon nearest the mouse within the pick radius wins.
        const ImVec2 mouse = ImGui::GetMousePos();
        float nearest = IconPickRadius * uiScale;

        for (const LightIcon& icon : lightIcons_)
        {
            const float distance = std::hypot(icon.screen.x - mouse.x, icon.screen.y - mouse.y);

            if (distance < nearest)
            {
                nearest = distance;
                hoveredLight_ = icon.entity;
            }
        }
    }

    void ViewportPanel::HandleClick(EditorContext& context, const ViewportProjection& projection, bool gizmoActive)
    {
        const ImGuiIO& io = ImGui::GetIO();

        // The pick uses the press position: what was under the cursor when the click began.
        if (imageHovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !gizmoActive)
        {
            clickStarted_ = true;
            clickPosition_ = io.MouseClickedPos[ImGuiMouseButton_Left];
            clickedLight_ = hoveredLight_;
        }

        if (!clickStarted_ || !ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            return;

        clickStarted_ = false;

        // A drag is a camera orbit or a gizmo edit, not a selection.
        if (io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] > ClickDragThreshold * ClickDragThreshold)
            return;

        std::optional<EntityId> hit = clickedLight_;

        if (!hit)
            hit = PickMesh(context.renderScene, projection.GetRay({ clickPosition_.x, clickPosition_.y }));

        EditorSelection& selection = context.document.GetSelection();

        if (!hit)
        {
            if (!io.KeyCtrl && !io.KeyShift)
                selection.Clear();
        }
        else if (io.KeyCtrl)
        {
            selection.Toggle(*hit);
        }
        else if (io.KeyShift)
        {
            selection.Add(*hit);
        }
        else
        {
            selection.Select(*hit);
        }
    }

    void ViewportPanel::HandleDrop(EditorContext& context, const ViewportProjection& projection)
    {
        const ImRect rect(imageMin_, ImVec2(imageMin_.x + imageSize_.x, imageMin_.y + imageSize_.y));

        if (!ImGui::BeginDragDropTargetCustom(rect, ImGui::GetID("ViewportDrop")))
            return;

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ATMOS_MODEL_PATH"))
        {
            const std::string path(static_cast<const char*>(payload->Data), payload->DataSize);
            const ImVec2 mouse = ImGui::GetMousePos();
            const Ray ray = projection.GetRay({ mouse.x, mouse.y });

            // Place it on whatever is under the cursor, or a few meters in front of the camera.
            float distance = 5.0f;
            PickMesh(context.renderScene, ray, &distance);
            distance = std::min(distance, 50.0f);

            XMFLOAT3 position;
            XMStoreFloat3(&position, XMLoadFloat3(&ray.origin) + XMLoadFloat3(&ray.direction) * distance);
            context.document.AddModel(ToWide(path), position);
        }

        ImGui::EndDragDropTarget();
    }

    void ViewportPanel::HandleBookmarks(EditorContext& context)
    {
        const ImGuiIO& io = ImGui::GetIO();

        if (!(imageHovered_ || focused_) || io.WantTextInput || context.cameraController.IsDragging() || io.KeyAlt)
            return;

        Scene& scene = context.document.GetScene();
        Camera& camera = context.camera;

        for (size_t slot = 0; slot < Scene::BookmarkCount; ++slot)
        {
            if (!ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + slot), false))
                continue;

            if (io.KeyCtrl)
            {
                scene.cameraBookmarks[slot] = SceneCamera{ camera.GetPosition(), camera.GetYawDegrees(),
                                                           camera.GetPitchDegrees(), camera.GetFovDegrees() };
                context.document.MarkEdited("Set Camera Bookmark");
            }
            else if (const std::optional<SceneCamera>& bookmark = scene.cameraBookmarks[slot])
            {
                camera.SetPosition(bookmark->position);
                camera.SetRotation(bookmark->yawDegrees, bookmark->pitchDegrees);
                camera.SetFov(bookmark->fovDegrees);
            }
        }
    }
}
