cmake_minimum_required(VERSION 3.24)

project(AtmosDX12 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(engine)
add_subdirectory(app)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT AtmosDX12)
