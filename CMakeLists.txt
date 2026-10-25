cmake_minimum_required(VERSION 3.21)

project(NslSnap VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6.8 LTS, installed separately (docs/07-development-setup.md).
set(NSL_QT_COMPONENTS Widgets)
if(LINUX)
    list(APPEND NSL_QT_COMPONENTS DBus)
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS ${NSL_QT_COMPONENTS} Test)
qt_standard_project_setup()

# Everything except main() lives in a library, so the tests can link it.
add_library(nsl_snap_core STATIC
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/capture/CaptureBackend.cpp
    src/capture/CaptureBackend.h
    src/capture/CaptureResult.h
    src/editor/EditorView.cpp
    src/editor/EditorView.h
    src/imaging/WindowImage.cpp
    src/imaging/WindowImage.h
)
target_include_directories(nsl_snap_core PUBLIC src)
target_link_libraries(nsl_snap_core PUBLIC Qt6::Widgets)

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
