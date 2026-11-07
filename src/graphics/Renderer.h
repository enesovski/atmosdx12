#pragma once

#include "d3d12/D3D12Context.h"
#include "d3d12/SwapChain.h"
#include "d3d12/FrameScheduler.h"
#include "passes/SkyPass.h"

struct RenderView;
struct SkySettings;

class Renderer
{
public:
    Renderer(HWND window, int width, int height);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void Render(const RenderView& view, const SkySettings& skySettings);

private:
    // Dependencies are constructed first and destroyed last.
    D3D12Context context_;
    SwapChain swapChain_;
    FrameScheduler frameScheduler_;
    SkyPass skyPass_;
};
