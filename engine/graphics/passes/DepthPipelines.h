#pragma once

#include "graphics/GpuScene.h"
#include "graphics/d3d12/PipelineState.h"

#include <array>
#include <string>

namespace atmos
{
    class CommandContext;
    class GraphicsDevice;
    class ShaderCompiler;

    struct DepthPipelineSettings
    {
        std::wstring name;
        int32_t depthBias = 0;
        float slopeScaledDepthBias = 0.0f;
        bool depthClip = true;
        bool cullBackFaces = true;
    };

    // Depth-only pipelines for the four depth-writing buckets (opaque/masked x single/double-sided),
    // shared by the camera prepass and the shadow passes.
    class DepthPipelines
    {
    public:
        DepthPipelines(const GraphicsDevice& device, const ShaderCompiler& compiler, const DepthPipelineSettings& settings);

        // Draws every depth-writing bucket of `list` into the bound depth target.
        void RecordDraws(CommandContext& commands, const GpuScene& scene, const DrawList& list, uint32_t viewBufferIndex,
                         uint32_t viewIndex) const;

    private:
        std::array<PipelineState, DepthBucketCount> pipelines_;
    };
}
