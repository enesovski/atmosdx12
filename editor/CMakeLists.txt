add_library(AtmosEditor STATIC
    ConsoleLog.h
    ConsoleLog.cpp
    EditorApp.h
    EditorApp.cpp
    EditorContext.h
    EditorLayout.h
    EditorLayout.cpp
    EditorPrefs.h
    EditorPrefs.cpp
    EditorSelection.h
    EditorSelection.cpp
    ImGuiLayer.h
    ImGuiLayer.cpp
    ProjectPaths.h
    ProjectPaths.cpp
    SceneDocument.h
    SceneDocument.cpp
    UndoStack.h
    UndoStack.cpp
    ViewModes.h
    ViewModes.cpp

    panels/ConsolePanel.h
    panels/ConsolePanel.cpp
    panels/ContentBrowserPanel.h
    panels/ContentBrowserPanel.cpp
    panels/HierarchyPanel.h
    panels/HierarchyPanel.cpp
    panels/InspectorPanel.h
    panels/InspectorPanel.cpp
    panels/Panel.h
    panels/Panel.cpp
    panels/PreferencesWindow.h
    panels/PreferencesWindow.cpp
    panels/ProfilerPanel.h
    panels/ProfilerPanel.cpp
    panels/RenderGraphPanel.h
    panels/RenderGraphPanel.cpp
    panels/RenderSettingsPanel.h
    panels/RenderSettingsPanel.cpp
    panels/ViewportPanel.h
    panels/ViewportPanel.cpp

    shell/EditorBars.h
    shell/EditorBars.cpp
    shell/MainMenuBar.h
    shell/MainMenuBar.cpp

    style/EditorFonts.h
    style/EditorFonts.cpp
    style/EditorTheme.h
    style/EditorTheme.cpp
    style/Icons.h

    tools/CameraController.h
    tools/CameraController.cpp
    tools/FileDialogs.h
    tools/FileDialogs.cpp
    tools/Gizmo.h
    tools/Gizmo.cpp
    tools/Picking.h
    tools/Picking.cpp

    widgets/PropertyGrid.h
    widgets/PropertyGrid.cpp
    widgets/Widgets.h
    widgets/Widgets.cpp
)

target_include_directories(AtmosEditor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(AtmosEditor PRIVATE /W4 /permissive- /utf-8)
endif()

include(ThirdParty.cmake)

target_link_libraries(AtmosEditor
    PUBLIC
        AtmosEngine
        imgui
    PRIVATE
        imguizmo
        nlohmann_json::nlohmann_json
        comdlg32
        shell32
)
