
cmake_minimum_required(VERSION 3.21)

project(AtmosDX12 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(AtmosDX12
    src/main.cpp
    src/Application.cpp
    src/Application.h
)

target_link_libraries(AtmosDX12 PRIVATE
    d3d12
    dxgi
    dxguid
)
