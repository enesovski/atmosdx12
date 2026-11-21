#ifndef ATMOS_VIEW_DATA_HLSLI
#define ATMOS_VIEW_DATA_HLSLI

// Mirrors ViewData in graphics/ShaderInterop.h. View 0 is the camera, then shadow cascades,
// then shadow-atlas tiles.
struct ViewData
{
    float4x4 viewProjection;
    float4 atlasScaleOffset;
    float3 position;
    float texelWorldSize;
};

ViewData LoadView(uint viewBufferIndex, uint viewIndex)
{
    StructuredBuffer<ViewData> views = ResourceDescriptorHeap[viewBufferIndex];
    return views[viewIndex];
}

#endif
