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
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/PropertiesPanel.cpp
    src/app/PropertiesPanel.h
    src/app/ToolRail.cpp
    src/app/ToolRail.h
    src/capture/CaptureBackend.cpp
    src/capture/CaptureBackend.h
    src/capture/CaptureResult.h
    src/editor/EditorView.cpp
    src/editor/EditorView.h
    src/editor/Tool.cpp
    src/editor/Tool.h
    src/imaging/WindowImage.cpp
    src/imaging/WindowImage.h
    src/ui/Icons.cpp
    src/ui/Icons.h
    src/ui/Theme.cpp
    src/ui/Theme.h
)
target_include_directories(nsl_snap_core PUBLIC src)
target_link_libraries(nsl_snap_core PUBLIC Qt6::Widgets Qt6::Svg)

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

qt_add_executable(nsl-snap src/main.cpp)
target_link_libraries(nsl-snap PRIVATE nsl_snap_core)

enable_testing()
add_subdirectory(tests)
