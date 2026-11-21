#pragma once

#include <DirectXMath.h>

namespace atmos::editor
{
    struct EditorContext;

    // Move/rotate/scale gizmo (ImGuizmo) for the primary selected entity, drawn over the viewport
    // image. Edits go through SceneDocument, so a whole drag is one undo step.
    // Returns true while the gizmo is hovered or dragged, so clicks do not select through it.
    bool DrawTransformGizmo(EditorContext& context, DirectX::XMFLOAT2 rectMin, DirectX::XMFLOAT2 rectSize);
}
