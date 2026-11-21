#include "tools/Gizmo.h"
#include "EditorContext.h"

#include <imgui.h>
#include <ImGuizmo.h>

using namespace DirectX;

namespace atmos::editor
{
    namespace
    {
        constexpr float GizmoFarPlane = 10000.0f;

        ImGuizmo::OPERATION ToOperation(GizmoTool tool)
        {
            switch (tool)
            {
                case GizmoTool::Rotate: return ImGuizmo::ROTATE;
                case GizmoTool::Scale:  return ImGuizmo::SCALE;
                default:                return ImGuizmo::TRANSLATE;
            }
        }

        const char* GetEditLabel(GizmoTool tool)
        {
            switch (tool)
            {
                case GizmoTool::Rotate: return "Rotate";
                case GizmoTool::Scale:  return "Scale";
                default:                return "Move";
            }
        }
    }

    bool DrawTransformGizmo(EditorContext& context, XMFLOAT2 rectMin, XMFLOAT2 rectSize)
    {
        Scene& scene = context.document.GetScene();
        const EntityId id = context.document.GetSelection().GetPrimary();

        if (context.gizmo.tool == GizmoTool::Select || !scene.IsValid(id) || rectSize.x < 1.0f || rectSize.y < 1.0f)
            return false;

        const std::vector<XMFLOAT4X4> world = scene.ComputeWorldMatrices();
        Entity& entity = scene.GetEntity(id);

        // ImGuizmo expects a conventional projection, not the renderer's reverse-Z infinite one.
        const Camera& camera = context.camera;
        XMFLOAT4X4 view, projection;
        XMStoreFloat4x4(&view, camera.GetViewMatrix());
        XMStoreFloat4x4(&projection, XMMatrixPerspectiveFovLH(XMConvertToRadians(camera.GetFovDegrees()),
                                                               rectSize.x / rectSize.y, camera.GetNearPlane(),
                                                               GizmoFarPlane));

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(rectMin.x, rectMin.y, rectSize.x, rectSize.y);
        ImGuizmo::PushID(static_cast<int>(id));

        const GizmoSettings& settings = context.gizmo;
        const float snapValue = settings.tool == GizmoTool::Rotate ? settings.rotateSnap
                              : settings.tool == GizmoTool::Scale  ? settings.scaleSnap
                                                                   : settings.moveSnap;
        const float snap[3] = { snapValue, snapValue, snapValue };

        // Scale always works in local space.
        const ImGuizmo::MODE mode = settings.space == GizmoSpace::Local || settings.tool == GizmoTool::Scale
                                        ? ImGuizmo::LOCAL
                                        : ImGuizmo::WORLD;

        XMFLOAT4X4 matrix = world[id];

        if (ImGuizmo::Manipulate(&view._11, &projection._11, ToOperation(settings.tool), mode, &matrix._11, nullptr,
                                 settings.snap ? snap : nullptr))
        {
            XMMATRIX local = XMLoadFloat4x4(&matrix);

            if (entity.parent != InvalidEntity)
                local = local * XMMatrixInverse(nullptr, XMLoadFloat4x4(&world[entity.parent]));

            const Transform transform = Transform::FromMatrix(local);

            // Only the edited component changes, so rotating never introduces scale drift and vice versa.
            switch (settings.tool)
            {
                case GizmoTool::Rotate: entity.transform.rotationDegrees = transform.rotationDegrees; break;
                case GizmoTool::Scale:  entity.transform.scale = transform.scale; break;
                default:                entity.transform.position = transform.position; break;
            }

            context.document.MarkEdited(GetEditLabel(settings.tool));
        }

        const bool active = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
        ImGuizmo::PopID();
        return active;
    }
}
