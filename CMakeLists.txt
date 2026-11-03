
cmake_minimum_required(VERSION 3.21)

project(AtmosDX12 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(AtmosDX12
    src/main.cpp
    src/Application.h
    src/Application.cpp

    src/core/FrameClock.h
    src/core/DxError.h

    src/platform/Window.h
    src/platform/Window.cpp

    src/input/Input.h

    src/scene/Camera.h
    src/scene/Camera.cpp

    src/graphics/D3D12Context.h
    src/graphics/D3D12Context.cpp
    src/graphics/SwapChain.h
    src/graphics/SwapChain.cpp
    src/graphics/FrameScheduler.h
    src/graphics/FrameScheduler.cpp
    src/graphics/FrameData.h
    src/graphics/GraphicsConfig.h
    src/graphics/Renderer.h
    src/graphics/Renderer.cpp
    src/graphics/GraphicsPipeline.h
    src/graphics/GraphicsPipeline.cpp
    src/graphics/UploadBuffer.h

    src/graphics/passes/SkyPass.h
    src/graphics/passes/SkyPass.cpp
)

target_compile_definitions(AtmosDX12 PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

target_link_libraries(AtmosDX12 PRIVATE
    d3d12
    dxgi
    dxguid
    d3dcompiler
)

add_custom_command(TARGET AtmosDX12 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:AtmosDX12>/shaders"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Sky.hlsl"
        "$<TARGET_FILE_DIR:AtmosDX12>/shaders/Sky.hlsl"

    VERBATIM
)
