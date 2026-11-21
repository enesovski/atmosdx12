#pragma once

#include "style/EditorTheme.h"

#include "graphics/OverlayRenderer.h"
#include "graphics/d3d12/DescriptorHeap.h"

#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace atmos
{
    class GraphicsDevice;
}

namespace atmos::editor
{
    // Dear ImGui (docking) with the Win32 + D3D12 backends, drawn as the renderer's overlay.
    // Owns the ImGui context, the font atlas and the theme. The GPU must be idle before destruction.
    class ImGuiLayer final : public IOverlayRenderer
    {
    public:
        // `layoutFile` stores the dock layout (ImGui ini).
        ImGuiLayer(HWND window, GraphicsDevice& device, const std::filesystem::path& layoutFile);
        ~ImGuiLayer() override;

        ImGuiLayer(const ImGuiLayer&) = delete;
        ImGuiLayer& operator=(const ImGuiLayer&) = delete;

        // Returns true if ImGui consumed the message.
        bool HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

        void BeginFrame();
        void EndFrame();

        // Rebuilds fonts and style when the scale, theme or font size changed. Rebuilding the font
        // atlas replaces a texture the GPU may still read, so the caller passes a wait callback.
        template <typename WaitForGpu>
        void SetAppearance(float scale, Theme theme, float fontSize, WaitForGpu&& waitForGpu)
        {
            if (scale == scale_ && theme == theme_ && fontSize == fontSize_)
                return;

            waitForGpu();
            ApplyAppearance(scale, theme, fontSize);
        }

        float GetScale() const { return scale_; }

        void RecordOverlay(CommandContext& commands) override;

    private:
        void ApplyAppearance(float scale, Theme theme, float fontSize);

        DescriptorHeap& shaderResourceHeap_;
        std::vector<Descriptor> descriptors_;  // allocated for the backend (font atlas)
        std::string layoutFile_;               // ImGui keeps a pointer to it
        float scale_ = 0.0f;
        Theme theme_ = Theme::Dark;
        float fontSize_ = 0.0f;
        bool fontsBuilt_ = false;
    };
}
