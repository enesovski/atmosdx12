#pragma once

#include "graphics/OverlayRenderer.h"
#include "graphics/d3d12/DescriptorHeap.h"

#include <windows.h>

#include <vector>

namespace atmos
{
    class GraphicsDevice;

    // Dear ImGui with the Win32 and D3D12 backends, drawn as the renderer's overlay.
    // Per frame: BeginFrame(), build windows with the ImGui API, EndFrame(), then pass this to
    // Renderer::Render. The GPU must be idle before destruction (Renderer::WaitIdle).
    class DebugUi final : public IOverlayRenderer
    {
    public:
        DebugUi(HWND window, GraphicsDevice& device);
        ~DebugUi() override;

        DebugUi(const DebugUi&) = delete;
        DebugUi& operator=(const DebugUi&) = delete;

        // Forward window messages; returns true if ImGui consumed the message.
        bool HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

        void BeginFrame();
        void EndFrame();

        // True while the cursor is over a UI window or a text field has focus.
        bool WantsMouse() const;
        bool WantsKeyboard() const;

        void RecordOverlay(CommandContext& commands) override;

    private:
        DescriptorHeap& shaderResourceHeap_;
        std::vector<Descriptor> descriptors_;  // allocated for the ImGui backend (font atlas)
    };
}
