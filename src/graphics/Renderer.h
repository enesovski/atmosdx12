#pragma once

#include "D3D12Context.h"
#include "SwapChain.h"
#include "FrameScheduler.h"
#include "passes/SkyPass.h"

struct FrameData;

class Renderer
{
public:
    Renderer(HWND window, int width, int height);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void Render(const FrameData& frameData);

private:
    // Dependencies are constructed first and destroyed last.
    D3D12Context context_;
    SwapChain swapChain_;
    FrameScheduler frameScheduler_;
    SkyPass skyPass_;

    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};
};
