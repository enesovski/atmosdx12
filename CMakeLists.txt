
cmake_minimum_required(VERSION 3.21)

project(AtmosDX12 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(AtmosDX12
    src/main.cpp
    src/Application.cpp
    src/Application.h
    src/graphics/D3D12Renderer.cpp
    src/graphics/D3D12Renderer.h
    src/graphics/GraphicsPipeline.cpp
    src/graphics/GraphicsPipeline.h
    src/graphics/passes/SkyPass.cpp
    src/graphics/passes/SkyPass.h
)

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
