#pragma once

#include "graphics/d3d12/PipelineState.h"

namespace atmos
{
    class CommandContext;
    class GraphicsDevice;
    class ShaderCompiler;
    class Texture;
    struct SkyEnvironment;

    // Fullscreen procedural sky with sun disk and glow, written to an HDR target.
    class SkyPass
    {
    public:
        SkyPass(const GraphicsDevice& device, const ShaderCompiler& compiler, DXGI_FORMAT outputFormat);

        void Record(CommandContext& context, Texture& output, const SkyEnvironment& sky) const;

    private:
        PipelineState pipeline_;
    };
}
