cmake_minimum_required(VERSION 3.21)

# macOS 14 or later (D-002: ScreenCaptureKit's SCScreenshotManager). Must be
# set before project(), which reads it.
if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Oldest macOS the app runs on")
endif()

project(Snapslash VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# `cmake --install`: the installed program keeps the path to the Qt it was
# built with (e.g. ~/Qt/6.8.3/gcc_64/lib). CMake would otherwise remove it,
# and the program would not start, or pick up the distribution's older Qt.
if(NOT DEFINED CMAKE_INSTALL_RPATH_USE_LINK_PATH)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
endif()

# Qt 6.8 LTS, installed separately (docs/07-development-setup.md).
# Network: QLocalSocket for the single instance and Hyprland's socket.
# Multimedia: encoding videos (F-50, D-039).
set(NSL_QT_COMPONENTS Widgets Svg Network Multimedia)
if(LINUX)
    list(APPEND NSL_QT_COMPONENTS DBus)
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS ${NSL_QT_COMPONENTS} Test LinguistTools)
qt_standard_project_setup()

# Everything except main() lives in a library, so the tests can link it.
add_library(nsl_snap_core STATIC
    src/app/ColorRow.cpp
    src/app/ColorRow.h
    src/app/CountdownPositionPicker.cpp
    src/app/CountdownPositionPicker.h
    src/app/CommandLine.cpp
    src/app/CommandLine.h
    src/app/FileNames.cpp
    src/app/FileNames.h
    src/app/GlobalHotkeys.cpp
    src/app/GlobalHotkeys.h
    src/app/GnomeExtensionSetup.cpp
    src/app/GnomeExtensionSetup.h
    src/app/Language.cpp
    src/app/Language.h
    src/app/LinuxIntegration.cpp
    src/app/LinuxIntegration.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/PlatformTheme.cpp
    src/app/PlatformTheme.h
    src/app/PropertiesPanel.cpp
    src/app/PropertiesPanel.h
    src/app/RecentCaptures.cpp
    src/app/RecentCaptures.h
    src/app/Settings.cpp
    src/app/Settings.h
    src/app/SettingsDialog.cpp
    src/app/SettingsDialog.h
    src/app/SingleInstance.cpp
    src/app/SingleInstance.h
    src/app/ToolRail.cpp
    src/app/ToolRail.h
    src/app/UpdateBar.cpp
    src/app/UpdateBar.h
    src/app/UpdateDialog.cpp
    src/app/UpdateDialog.h
    src/capture/CaptureBackend.cpp
    src/capture/CaptureHints.cpp
    src/capture/CaptureHints.h
    src/capture/CaptureBackend.h
    src/capture/CaptureOverlay.cpp
    src/capture/CaptureOverlay.h
    src/capture/CaptureResult.h
    src/capture/Countdown.cpp
    src/capture/CountdownOptions.cpp
    src/capture/CountdownOptions.h
    src/capture/Countdown.h
    src/capture/PickGeometry.cpp
    src/capture/PickGeometry.h
    src/capture/RegionSelector.cpp
    src/capture/RegionSelector.h
    src/editor/ArrowItem.cpp
    src/editor/ArrowItem.h
    src/editor/ArrowStyle.h
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
    src/editor/MarkerItem.cpp
    src/editor/MarkerItem.h
    src/editor/MarkerStyle.h
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
    src/recording/RecordingBackend.cpp
    src/recording/RecordingBackend.h
    src/recording/RecordingController.cpp
    src/recording/RecordingController.h
    src/recording/RecordingIndicator.cpp
    src/recording/RecordingIndicator.h
    src/recording/TrimTimeline.cpp
    src/recording/TrimTimeline.h
    src/recording/TrimWindow.cpp
    src/recording/TrimWindow.h
    src/recording/VideoEncoder.cpp
    src/recording/VideoEncoder.h
    src/recording/VideoFrames.cpp
    src/recording/VideoFrames.h
    src/recording/VideoTrimmer.cpp
    src/recording/VideoTrimmer.h
    src/ui/AppDialog.cpp
    src/ui/AppDialog.h
    src/ui/ColorScheme.cpp
    src/ui/ColorScheme.h
    src/ui/ColorPopup.cpp
    src/ui/ColorPopup.h
    src/ui/ColorWheel.cpp
    src/ui/ColorWheel.h
    src/ui/Icons.cpp
    src/ui/Icons.h
    src/ui/LogoMark.cpp
    src/ui/LogoMark.h
    src/ui/Motion.cpp
    src/ui/Motion.h
    src/ui/Placement.cpp
    src/ui/Placement.h
    src/ui/Theme.cpp
    src/ui/Theme.h
    src/update/Releases.cpp
    src/update/Releases.h
    src/update/UpdateChecker.cpp
    src/update/UpdateChecker.h
    src/update/UpdateInstaller.cpp
    src/update/UpdateInstaller.h
    src/update/Version.cpp
    src/update/Version.h
)
target_include_directories(nsl_snap_core PUBLIC src)
# The version the app reports and compares with the releases (D-037). The
# release workflow sets the suffix for pre-releases (tag v0.3.0-beta.1).
set(NSL_VERSION_SUFFIX "" CACHE STRING "Pre-release part of the version, e.g. beta.1")
set(NSL_VERSION "${PROJECT_VERSION}")
if(NSL_VERSION_SUFFIX)
    string(APPEND NSL_VERSION "-${NSL_VERSION_SUFFIX}")
endif()
target_compile_definitions(nsl_snap_core PUBLIC NSL_VERSION="${NSL_VERSION}")
target_link_libraries(nsl_snap_core PUBLIC Qt6::Widgets Qt6::Svg Qt6::Network Qt6::Multimedia)

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

# The script that swaps the app folder on Windows (UpdateInstaller, D-037).
qt_add_resources(nsl_snap_core "update"
    PREFIX "/update"
    BASE "resources/update"
    FILES resources/update/swap-folder.ps1
)

# Platform capture backends (docs/01-architecture.md).
if(LINUX)
    target_sources(nsl_snap_core PRIVATE
        src/capture/gnome/GnomeShellBackend.cpp
        src/capture/gnome/GnomeShellBackend.h
        src/ui/linux/PortalColorScheme.cpp
        src/ui/linux/PortalColorScheme.h
    )
    target_link_libraries(nsl_snap_core PUBLIC Qt6::DBus)
    target_compile_definitions(nsl_snap_core PUBLIC NSL_HAVE_GNOME_BACKEND)

    # Wayland without GNOME (D-031, D-032): our own Wayland client code for
    # wlr-screencopy and hyprland-toplevel-export, and the desktop portal.
    # wayland-scanner turns the protocol files into C code.
    enable_language(C)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
    set(NSL_PROTOCOL_DIR ${CMAKE_CURRENT_BINARY_DIR}/protocols)
    file(MAKE_DIRECTORY ${NSL_PROTOCOL_DIR})
    foreach(protocol
            wlr-screencopy-unstable-v1
            wlr-foreign-toplevel-management-unstable-v1
            hyprland-toplevel-export-v1)
        set(xml ${CMAKE_CURRENT_SOURCE_DIR}/protocols/${protocol}.xml)
        add_custom_command(
            OUTPUT ${NSL_PROTOCOL_DIR}/${protocol}-client-protocol.h ${NSL_PROTOCOL_DIR}/${protocol}-protocol.c
            COMMAND ${WAYLAND_SCANNER} client-header ${xml} ${NSL_PROTOCOL_DIR}/${protocol}-client-protocol.h
            COMMAND ${WAYLAND_SCANNER} private-code ${xml} ${NSL_PROTOCOL_DIR}/${protocol}-protocol.c
            DEPENDS ${xml}
            VERBATIM)
        target_sources(nsl_snap_core PRIVATE
            ${NSL_PROTOCOL_DIR}/${protocol}-client-protocol.h
            ${NSL_PROTOCOL_DIR}/${protocol}-protocol.c)
    endforeach()
    target_include_directories(nsl_snap_core PRIVATE ${NSL_PROTOCOL_DIR})
    target_sources(nsl_snap_core PRIVATE
        src/capture/portal/PortalBackend.cpp
        src/capture/portal/PortalBackend.h
        src/capture/wayland/HyprlandIpc.cpp
        src/capture/wayland/HyprlandIpc.h
        src/capture/wayland/LayerShell.cpp
        src/capture/wayland/LayerShell.h
        src/capture/wayland/WaylandBackend.cpp
        src/capture/wayland/WaylandBackend.h
        src/capture/wayland/WaylandConnection.cpp
        src/capture/wayland/WaylandConnection.h
        src/capture/wayland/WaylandFreeze.cpp
        src/capture/wayland/WaylandFreeze.h
        src/recording/wayland/WaylandRecordingBackend.cpp
        src/recording/wayland/WaylandRecordingBackend.h
        src/recording/wayland/WaylandStream.cpp
        src/recording/wayland/WaylandStream.h
    )
    target_link_libraries(nsl_snap_core PUBLIC PkgConfig::WAYLAND_CLIENT)
    target_compile_definitions(nsl_snap_core PUBLIC NSL_HAVE_WAYLAND_BACKEND)

    # Videos on GNOME (D-039): Mutter's screen cast, read from PipeWire.
    # Optional: without libpipewire the app records no videos on GNOME.
    pkg_check_modules(PIPEWIRE IMPORTED_TARGET libpipewire-0.3)
    if(PIPEWIRE_FOUND)
        target_sources(nsl_snap_core PRIVATE
            src/recording/gnome/GnomeRecordingBackend.cpp
            src/recording/gnome/GnomeRecordingBackend.h
            src/recording/pipewire/PipeWireStream.cpp
            src/recording/pipewire/PipeWireStream.h
        )
        target_link_libraries(nsl_snap_core PUBLIC PkgConfig::PIPEWIRE)
        target_compile_definitions(nsl_snap_core PUBLIC NSL_HAVE_PIPEWIRE)
        message(STATUS "PipeWire found: videos on GNOME")
    else()
        message(STATUS "PipeWire not found: no videos on GNOME")
    endif()

    # Optional: KDE's LayerShellQt puts the overlay above all windows on
    # Hyprland, Sway & co. Without it the overlay is shown full screen.
    find_package(LayerShellQt 6 CONFIG QUIET)
    if(LayerShellQt_FOUND)
        target_link_libraries(nsl_snap_core PRIVATE LayerShellQt::Interface)
        target_compile_definitions(nsl_snap_core PRIVATE NSL_HAVE_LAYER_SHELL_QT)
        # 6.6 replaced setScreenConfiguration() with setScreen().
        if(LayerShellQt_VERSION VERSION_GREATER_EQUAL 6.6)
            target_compile_definitions(nsl_snap_core PRIVATE NSL_LAYER_SHELL_QT_SET_SCREEN)
        endif()
        message(STATUS "LayerShellQt found: overlay as layer surface on wlroots compositors")
    else()
        message(STATUS "LayerShellQt not found: overlay full screen on wlroots compositors")
    endif()
endif()
if(WIN32)
    # Windows.Graphics.Capture through C++/WinRT, Direct3D 11 and DWM (D-022).
    target_sources(nsl_snap_core PRIVATE
        src/capture/windows/WgcCapture.cpp
        src/capture/windows/WgcCapture.h
        src/capture/windows/WindowsBackend.cpp
        src/capture/windows/WindowsBackend.h
        src/recording/windows/WindowsRecordingBackend.cpp
        src/recording/windows/WindowsRecordingBackend.h
    )
    # GuiPrivate: the monitors' physical geometry (QPlatformScreen).
    target_link_libraries(nsl_snap_core PUBLIC Qt6::GuiPrivate d3d11 dxgi dwmapi windowsapp)
    target_compile_definitions(nsl_snap_core PUBLIC NSL_HAVE_WINDOWS_BACKEND)
endif()

if(APPLE)
    # ScreenCaptureKit and AppKit through Objective-C++ (D-002, D-047),
    # with automatic reference counting for the Objective-C objects; Carbon
    # for the global hotkeys (D-048).
    enable_language(OBJCXX)
    target_sources(nsl_snap_core PRIVATE
        src/capture/macos/MacBackend.cpp
        src/capture/macos/MacBackend.h
        src/capture/macos/MacCapture.h
        src/capture/macos/MacCapture.mm
        src/recording/macos/MacRecordingBackend.cpp
        src/recording/macos/MacRecordingBackend.h
    )
    set_source_files_properties(src/capture/macos/MacCapture.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(nsl_snap_core PUBLIC
        "-framework AppKit" "-framework Carbon" "-framework CoreGraphics" "-framework CoreMedia"
        "-framework CoreVideo" "-framework ScreenCaptureKit")
    target_compile_definitions(nsl_snap_core PUBLIC NSL_HAVE_MACOS_BACKEND)
endif()

# Translations (D-038): the texts in the code are German; lrelease turns
# translations/*.ts into .qm files in the resources (":/i18n/"), which
# Language::install() loads. scripts/update-translations.sh refreshes the
# .ts files from the code with lupdate.
qt_add_lrelease(TS_FILES translations/snapslash_en.ts QM_FILES_OUTPUT_VARIABLE NSL_QM_FILES)
qt_add_resources(nsl_snap_core "translations"
    PREFIX "/i18n"
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${NSL_QM_FILES}
)

qt_add_executable(snapslash src/main.cpp)
if(WIN32)
    # A GUI program: no console window next to the app (main.cpp borrows the
    # terminal it was started from, if any). The resource file gives the
    # .exe its icon in Explorer and the taskbar (D-036).
    set_target_properties(snapslash PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(snapslash PRIVATE resources/windows/snapslash.rc)
endif()
if(APPLE)
    # An app bundle, Snapslash.app, with its icon and Info.plist (D-046).
    # The icon is resources/icons/logo.svg on Apple's grid (824 of 1024 px).
    set(NSL_MACOS_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/Snapslash.icns)
    set_source_files_properties(${NSL_MACOS_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(snapslash PRIVATE ${NSL_MACOS_ICON})
    set_target_properties(snapslash PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME Snapslash
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/Info.plist.in
        MACOSX_BUNDLE_GUI_IDENTIFIER ch.netslash.Snapslash
        MACOSX_BUNDLE_BUNDLE_NAME Snapslash
        MACOSX_BUNDLE_ICON_FILE Snapslash.icns
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2026 Michael Pfister, MIT License")
endif()
target_link_libraries(snapslash PRIVATE nsl_snap_core)

# `cmake --install`, used by packages (e.g. the Nix package in nix/).
include(GNUInstallDirs)
install(TARGETS snapslash
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .)
if(LINUX)
    # Desktop entry and icon, so launchers list the app (D-036). The icon
    # name matches the app id (QGuiApplication::desktopFileName in main.cpp).
    install(FILES resources/ch.netslash.Snapslash.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/icons/logo.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME ch.netslash.Snapslash.svg)
endif()

enable_testing()
add_subdirectory(tests)
