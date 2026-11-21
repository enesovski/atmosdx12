# Third-party libraries, fetched at configure time and pinned to release tags.
include(FetchContent)

# cgltf: single-header glTF parser. SOURCE_SUBDIR points nowhere so only sources are downloaded.
FetchContent_Declare(cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG v1.15
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-add-subdirectory
)

# nlohmann/json: header-only JSON, from the small release archive instead of the full repository.
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# DirectXTex: texture loading (WIC/DDS), mip generation and BC compression.
# Our own thread pool parallelizes across textures, so OpenMP stays off.
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_DX11 OFF CACHE BOOL "" FORCE)
set(BUILD_DX12 OFF CACHE BOOL "" FORCE)
set(BC_USE_OPENMP OFF CACHE BOOL "" FORCE)
FetchContent_Declare(DirectXTex
    GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
    GIT_TAG oct2025
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(cgltf nlohmann_json)

# BC7 compression is orders of magnitude slower unoptimized, so DirectXTex is built with
# optimizations even in Debug (same debug CRT; only its own code is optimized).
block(SCOPE_FOR VARIABLES)
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /O2 /Ob2")
    FetchContent_MakeAvailable(DirectXTex)
endblock()
