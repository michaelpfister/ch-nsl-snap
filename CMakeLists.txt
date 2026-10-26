cmake_minimum_required(VERSION 3.21)

project(NslSnap VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6.8 LTS, installed separately (docs/07-development-setup.md).
set(NSL_QT_COMPONENTS Widgets Svg)
if(LINUX)
    list(APPEND NSL_QT_COMPONENTS DBus)
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS ${NSL_QT_COMPONENTS} Test)
qt_standard_project_setup()

# Everything except main() lives in a library, so the tests can link it.
add_library(nsl_snap_core STATIC
    src/app/ColorRow.cpp
    src/app/ColorRow.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/PropertiesPanel.cpp
    src/app/PropertiesPanel.h
    src/app/Settings.cpp
    src/app/Settings.h
    src/app/SettingsDialog.cpp
    src/app/SettingsDialog.h
    src/app/ToolRail.cpp
    src/app/ToolRail.h
    src/capture/CaptureBackend.cpp
    src/capture/CaptureBackend.h
    src/capture/CaptureOverlay.cpp
    src/capture/CaptureOverlay.h
    src/capture/CaptureResult.h
    src/capture/Countdown.cpp
    src/capture/Countdown.h
    src/capture/PickGeometry.cpp
    src/capture/PickGeometry.h
    src/capture/RegionSelector.cpp
    src/capture/RegionSelector.h
    src/editor/Commands.cpp
    src/editor/Commands.h
    src/editor/DimItem.cpp
    src/editor/DimItem.h
    src/editor/DimSettings.h
    src/editor/EditorScene.cpp
    src/editor/EditorScene.h
    src/editor/EditorView.cpp
    src/editor/EditorView.h
    src/editor/FrameItem.cpp
    src/editor/FrameItem.h
    src/editor/FrameStyle.h
    src/editor/RectItem.cpp
    src/editor/RectItem.h
    src/editor/RedactItem.cpp
    src/editor/RedactItem.h
    src/editor/RedactStyle.h
    src/editor/ReplaceItem.cpp
    src/editor/ReplaceItem.h
    src/editor/ReplaceStyle.h
    src/editor/TextItem.cpp
    src/editor/TextItem.h
    src/editor/TextStyle.h
    src/editor/Tool.cpp
    src/editor/Tool.h
    src/imaging/Blur.cpp
    src/imaging/Blur.h
    src/imaging/TextSample.cpp
    src/imaging/TextSample.h
    src/imaging/WindowImage.cpp
    src/imaging/WindowImage.h
    src/ui/ColorPopup.cpp
    src/ui/ColorPopup.h
    src/ui/ColorWheel.cpp
    src/ui/ColorWheel.h
    src/ui/Icons.cpp
    src/ui/Icons.h
    src/ui/Theme.cpp
    src/ui/Theme.h
)
target_include_directories(nsl_snap_core PUBLIC src)
target_link_libraries(nsl_snap_core PUBLIC Qt6::Widgets Qt6::Svg)

# Image processing runs over millions of pixels; keep it fast in debug
# builds too, otherwise the blur takes half a second (F-27).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(src/imaging/Blur.cpp src/imaging/WindowImage.cpp
        PROPERTIES COMPILE_OPTIONS "-O2")
endif()

file(GLOB NSL_ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} resources/icons/*.svg)
qt_add_resources(nsl_snap_core "icons"
    PREFIX "/icons"
    BASE "resources/icons"
    FILES ${NSL_ICONS}
)

# Platform capture backends (docs/01-architecture.md).
if(LINUX)
    target_sources(nsl_snap_core PRIVATE
        src/capture/gnome/GnomeShellBackend.cpp
        src/capture/gnome/GnomeShellBackend.h
    )
    target_link_libraries(nsl_snap_core PUBLIC Qt6::DBus)
    target_compile_definitions(nsl_snap_core PUBLIC NSL_HAVE_GNOME_BACKEND)
endif()
if(WIN32)
    # Windows.Graphics.Capture through C++/WinRT, Direct3D 11 and DWM (D-022).
    target_sources(nsl_snap_core PRIVATE
        src/capture/windows/WgcCapture.cpp
        src/capture/windows/WgcCapture.h
        src/capture/windows/WindowsBackend.cpp
        src/capture/windows/WindowsBackend.h
    )
    # GuiPrivate: the monitors' physical geometry (QPlatformScreen).
    target_link_libraries(nsl_snap_core PUBLIC Qt6::GuiPrivate d3d11 dxgi dwmapi windowsapp)
    target_compile_definitions(nsl_snap_core PUBLIC NSL_HAVE_WINDOWS_BACKEND)
endif()

qt_add_executable(nsl-snap src/main.cpp)
target_link_libraries(nsl-snap PRIVATE nsl_snap_core)

enable_testing()
add_subdirectory(tests)
