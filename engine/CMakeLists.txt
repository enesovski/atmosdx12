add_library(AtmosEngine STATIC
    core/Assert.h
    core/Assert.cpp
    core/Build.h
    core/FrameTimer.h
    core/Handle.h
    core/Log.h
    core/Log.cpp
    core/StringUtils.h
    core/StringUtils.cpp
    core/ThreadPool.h
    core/ThreadPool.cpp

    platform/Input.h
    platform/Paths.h
    platform/Paths.cpp
    platform/Window.h
    platform/Window.cpp

    scene/Camera.h
    scene/Camera.cpp
    scene/Scene.h
    scene/Scene.cpp
    scene/SceneSerializer.h
    scene/SceneSerializer.cpp

    assets/AssetCache.h
    assets/AssetCache.cpp
    assets/CgltfImplementation.cpp
    assets/GltfLoader.h
    assets/GltfLoader.cpp
    assets/ModelCooker.h
    assets/ModelCooker.cpp
    assets/ModelData.h
    assets/PrimitiveMeshes.h
    assets/PrimitiveMeshes.cpp
    assets/TextureCooker.h
    assets/TextureCooker.cpp
    assets/TextureData.h

    graphics/d3d12/BindlessRootSignature.h
    graphics/d3d12/BindlessRootSignature.cpp
    graphics/d3d12/Buffer.h
    graphics/d3d12/Buffer.cpp
    graphics/d3d12/CommandContext.h
    graphics/d3d12/CommandContext.cpp
    graphics/d3d12/D3D12Utils.h
    graphics/d3d12/D3D12Utils.cpp
    graphics/d3d12/DeferredReleaseQueue.h
    graphics/d3d12/DeferredReleaseQueue.cpp
    graphics/d3d12/DescriptorHeap.h
    graphics/d3d12/DescriptorHeap.cpp
    graphics/d3d12/FrameScheduler.h
    graphics/d3d12/FrameScheduler.cpp
    graphics/d3d12/FrameStructuredBuffer.h
    graphics/d3d12/GpuTimer.h
    graphics/d3d12/GpuTimer.cpp
    graphics/d3d12/GraphicsConfig.h
    graphics/d3d12/GraphicsDevice.h
    graphics/d3d12/GraphicsDevice.cpp
    graphics/d3d12/GraphicsDeviceOptions.h
    graphics/d3d12/PipelineState.h
    graphics/d3d12/PipelineState.cpp
    graphics/d3d12/ShaderCompiler.h
    graphics/d3d12/ShaderCompiler.cpp
    graphics/d3d12/SwapChain.h
    graphics/d3d12/SwapChain.cpp
    graphics/d3d12/Texture.h
    graphics/d3d12/Texture.cpp
    graphics/d3d12/UploadAllocator.h
    graphics/d3d12/UploadAllocator.cpp
    graphics/d3d12/UploadQueue.h
    graphics/d3d12/UploadQueue.cpp

    graphics/rendergraph/RenderGraph.h
    graphics/rendergraph/RenderGraph.cpp
    graphics/rendergraph/TransientResourcePool.h
    graphics/rendergraph/TransientResourcePool.cpp

    graphics/lighting/LightingTypes.h
    graphics/lighting/LightingTypes.cpp
    graphics/lighting/LightSetup.h
    graphics/lighting/LightSetup.cpp
    graphics/lighting/SkyLight.h
    graphics/lighting/SkyLight.cpp

    graphics/passes/DeferredLightingPass.h
    graphics/passes/DeferredLightingPass.cpp
    graphics/passes/DepthPipelines.h
    graphics/passes/DepthPipelines.cpp
    graphics/passes/DepthPrepass.h
    graphics/passes/DepthPrepass.cpp
    graphics/passes/GBufferPass.h
    graphics/passes/GBufferPass.cpp
    graphics/passes/LightClusteringPass.h
    graphics/passes/LightClusteringPass.cpp
    graphics/passes/ShadowPass.h
    graphics/passes/ShadowPass.cpp
    graphics/passes/SkyPass.h
    graphics/passes/SkyPass.cpp
    graphics/passes/TonemapPass.h
    graphics/passes/TonemapPass.cpp
    graphics/passes/TransparentPass.h
    graphics/passes/TransparentPass.cpp

    graphics/Frustum.h
    graphics/Frustum.cpp
    graphics/GpuScene.h
    graphics/GpuScene.cpp
    graphics/Material.h
    graphics/MaterialData.h
    graphics/Mesh.h
    graphics/Mesh.cpp
    graphics/MeshData.h
    graphics/OverlayRenderer.h
    graphics/RenderScene.h
    graphics/RenderSettings.h
    graphics/RenderView.h
    graphics/RenderView.cpp
    graphics/Renderer.h
    graphics/Renderer.cpp
    graphics/SceneRenderer.h
    graphics/SceneRenderer.cpp
    graphics/ShaderInterop.h

    runtime/AssetManager.h
    runtime/AssetManager.cpp
    runtime/RenderSceneBuilder.h
    runtime/RenderSceneBuilder.cpp

    ui/DebugUi.h
    ui/DebugUi.cpp
)

target_include_directories(AtmosEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(AtmosEngine PUBLIC
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(AtmosEngine PRIVATE /W4 /permissive- /utf-8)
endif()

include(ThirdParty.cmake)

target_include_directories(AtmosEngine SYSTEM PRIVATE ${cgltf_SOURCE_DIR})

target_link_libraries(AtmosEngine
    PUBLIC
        d3d12
        dxgi
        dxguid
        dxcompiler
        imgui
    PRIVATE
        DirectXTex
        nlohmann_json::nlohmann_json
)

# DXC runtime DLLs ship with the Windows SDK and must sit next to the executable.
if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    set(ATMOS_SDK_VERSION "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
else()
    string(REGEX REPLACE "\\\\$" "" ATMOS_SDK_VERSION "$ENV{WindowsSDKVersion}")
endif()

cmake_host_system_information(RESULT ATMOS_KITS_ROOT
    QUERY WINDOWS_REGISTRY "HKLM/SOFTWARE/Microsoft/Windows Kits/Installed Roots"
    VALUE "KitsRoot10")

set(ATMOS_SDK_BIN_DIR "${ATMOS_KITS_ROOT}bin/${ATMOS_SDK_VERSION}/x64")
find_file(ATMOS_DXCOMPILER_DLL dxcompiler.dll PATHS "${ATMOS_SDK_BIN_DIR}" NO_DEFAULT_PATH REQUIRED)
find_file(ATMOS_DXIL_DLL dxil.dll PATHS "${ATMOS_SDK_BIN_DIR}" NO_DEFAULT_PATH REQUIRED)

file(GLOB_RECURSE ATMOS_SHADER_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.hlsl*")
set_source_files_properties(${ATMOS_SHADER_FILES} PROPERTIES HEADER_FILE_ONLY ON)

# Copies engine shaders and DXC DLLs next to an executable that links AtmosEngine.
function(atmos_add_runtime_files target)
    add_custom_target(${target}RuntimeFiles ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shaders"
            "$<TARGET_FILE_DIR:${target}>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ATMOS_DXCOMPILER_DLL}"
            "${ATMOS_DXIL_DLL}"
            "$<TARGET_FILE_DIR:${target}>"
        SOURCES ${ATMOS_SHADER_FILES}
        VERBATIM
    )
    add_dependencies(${target} ${target}RuntimeFiles)
endfunction()
