#pragma once

#include "graphics/d3d12/PipelineState.h"

namespace atmos
{
    class CommandContext;
    class GraphicsDevice;
    class ShaderCompiler;
    class Texture;
    struct PostProcessSettings;

    // Exposure + tone curve + sRGB encode from the HDR scene color to a display target.
    class TonemapPass
    {
    public:
        TonemapPass(const GraphicsDevice& device, const ShaderCompiler& compiler, DXGI_FORMAT outputFormat);

        void Record(CommandContext& context, Texture& hdrInput, Texture& output,
                    const PostProcessSettings& settings) const;

    private:
        PipelineState pipeline_;
    };
}
