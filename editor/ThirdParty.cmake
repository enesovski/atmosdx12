# Editor-only third-party libraries, fetched at configure time and pinned. The engine never uses them.
include(FetchContent)

# Dear ImGui, docking branch: dockable panels. It has no CMake project, so the library is defined below.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b-docking
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-add-subdirectory
)

# ImGuizmo: transform gizmos drawn through ImGui. Pinned to a commit (no recent tags): the last one
# before ImGuizmo required ImGui 1.92.8 draw-list signatures.
FetchContent_Declare(imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG 22a6c900a33dcd7a72576bddc7bd21651a2deed4
    SOURCE_SUBDIR do-not-add-subdirectory
)

FetchContent_MakeAvailable(imgui imguizmo)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)
target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(imgui PUBLIC d3d12 dxgi d3dcompiler)

add_library(imguizmo STATIC ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp)
target_include_directories(imguizmo SYSTEM PUBLIC ${imguizmo_SOURCE_DIR}/src)
target_link_libraries(imguizmo PUBLIC imgui)
