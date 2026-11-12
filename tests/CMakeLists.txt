# Tests that need widgets or fonts draw offscreen, without a display. On
# Windows the offscreen platform loads fonts with FreeType from Qt's own
# lib/fonts folder, which Qt does not ship; point it to the system fonts.
set(NSL_OFFSCREEN_ENV "QT_QPA_PLATFORM=offscreen")
if(WIN32)
    file(TO_CMAKE_PATH "$ENV{WINDIR}/Fonts" NSL_SYSTEM_FONTS)
    list(APPEND NSL_OFFSCREEN_ENV "QT_QPA_FONTDIR=${NSL_SYSTEM_FONTS}")
endif()

qt_add_executable(tst_windowimage tst_windowimage.cpp)
target_link_libraries(tst_windowimage PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_windowimage COMMAND tst_windowimage)

qt_add_executable(tst_editor tst_editor.cpp)
target_link_libraries(tst_editor PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_editor COMMAND tst_editor)
set_tests_properties(tst_editor PROPERTIES ENVIRONMENT "${NSL_OFFSCREEN_ENV}")

qt_add_executable(tst_blur tst_blur.cpp)
target_link_libraries(tst_blur PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_blur COMMAND tst_blur)

qt_add_executable(tst_view tst_view.cpp)
target_link_libraries(tst_view PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_view COMMAND tst_view)
set_tests_properties(tst_view PROPERTIES ENVIRONMENT "${NSL_OFFSCREEN_ENV}")

qt_add_executable(tst_capture tst_capture.cpp)
target_link_libraries(tst_capture PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_capture COMMAND tst_capture)

qt_add_executable(tst_overlay tst_overlay.cpp)
target_link_libraries(tst_overlay PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_overlay COMMAND tst_overlay)
set_tests_properties(tst_overlay PROPERTIES ENVIRONMENT "${NSL_OFFSCREEN_ENV}")

# Videos (F-50, D-039): frames, encoder, the flow with a fake backend.
qt_add_executable(tst_recording tst_recording.cpp)
target_link_libraries(tst_recording PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_recording COMMAND tst_recording)
set_tests_properties(tst_recording PROPERTIES ENVIRONMENT "${NSL_OFFSCREEN_ENV}")

# Registers real hotkeys with Windows (F-42); offscreen is enough.
qt_add_executable(tst_hotkeys tst_hotkeys.cpp)
target_link_libraries(tst_hotkeys PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_hotkeys COMMAND tst_hotkeys)
set_tests_properties(tst_hotkeys PROPERTIES ENVIRONMENT "${NSL_OFFSCREEN_ENV}")

# Capture on Wayland without GNOME (D-031, D-032): the parts that need no
# compositor.
if(LINUX)
    qt_add_executable(tst_wayland tst_wayland.cpp)
    target_link_libraries(tst_wayland PRIVATE nsl_snap_core Qt6::Test)
    add_test(NAME tst_wayland COMMAND tst_wayland)
    set_tests_properties(tst_wayland PROPERTIES ENVIRONMENT "${NSL_OFFSCREEN_ENV}")
endif()

# Captures real windows with Windows.Graphics.Capture: needs a desktop
# session, so it uses the normal "windows" platform, not offscreen.
if(WIN32)
    qt_add_executable(tst_wincapture tst_wincapture.cpp)
    target_link_libraries(tst_wincapture PRIVATE nsl_snap_core Qt6::Test)
    add_test(NAME tst_wincapture COMMAND tst_wincapture)
endif()

# On Windows qt_add_executable builds GUI programs, whose Qt Test log goes
# to the debugger instead of the console; tests must be console programs,
# so ctest --output-on-failure shows what failed.
if(WIN32)
    foreach(test_target tst_windowimage tst_editor tst_blur tst_view tst_capture tst_overlay tst_wincapture tst_hotkeys tst_recording)
        set_target_properties(${test_target} PROPERTIES WIN32_EXECUTABLE FALSE)
    endforeach()
endif()

qt_add_executable(tst_appdialog tst_appdialog.cpp)
target_link_libraries(tst_appdialog PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_appdialog COMMAND tst_appdialog)
set_tests_properties(tst_appdialog PROPERTIES ENVIRONMENT "${NSL_OFFSCREEN_ENV}")
if(WIN32)
    set_target_properties(tst_appdialog PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

qt_add_executable(tst_logomark tst_logomark.cpp)
target_link_libraries(tst_logomark PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_logomark COMMAND tst_logomark)
set_tests_properties(tst_logomark PROPERTIES ENVIRONMENT "${NSL_OFFSCREEN_ENV}")
if(WIN32)
    set_target_properties(tst_logomark PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

qt_add_executable(tst_settingsoverlay tst_settingsoverlay.cpp)
target_link_libraries(tst_settingsoverlay PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_settingsoverlay COMMAND tst_settingsoverlay)
set_tests_properties(tst_settingsoverlay PROPERTIES ENVIRONMENT "${NSL_OFFSCREEN_ENV}")
if(WIN32)
    set_target_properties(tst_settingsoverlay PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

qt_add_executable(tst_colorscheme tst_colorscheme.cpp)
target_link_libraries(tst_colorscheme PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_colorscheme COMMAND tst_colorscheme)
set_tests_properties(tst_colorscheme PROPERTIES ENVIRONMENT "${NSL_OFFSCREEN_ENV}")
if(WIN32)
    set_target_properties(tst_colorscheme PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

qt_add_executable(tst_colors tst_colors.cpp)
target_link_libraries(tst_colors PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_colors COMMAND tst_colors)
set_tests_properties(tst_colors PROPERTIES ENVIRONMENT "${NSL_OFFSCREEN_ENV}")

qt_add_executable(tst_commandline tst_commandline.cpp)
target_link_libraries(tst_commandline PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_commandline COMMAND tst_commandline)
if(WIN32)
    set_target_properties(tst_commandline PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

qt_add_executable(tst_filenames tst_filenames.cpp)
target_link_libraries(tst_filenames PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_filenames COMMAND tst_filenames)

# Updates and downgrades (F-49, D-037); on Windows also the folder swap.
qt_add_executable(tst_update tst_update.cpp)
target_link_libraries(tst_update PRIVATE nsl_snap_core Qt6::Test)
target_compile_definitions(tst_update PRIVATE NSL_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME tst_update COMMAND tst_update)
set_tests_properties(tst_update PROPERTIES ENVIRONMENT "${NSL_OFFSCREEN_ENV}")
if(WIN32)
    set_target_properties(tst_update PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

# The user interface in German and English (D-038).
qt_add_executable(tst_language tst_language.cpp)
target_link_libraries(tst_language PRIVATE nsl_snap_core Qt6::Test)
target_compile_definitions(tst_language PRIVATE NSL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_test(NAME tst_language COMMAND tst_language)
if(WIN32)
    set_target_properties(tst_language PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

# Series of captures and the list of recent ones (F-52), the countdown's
# position in the settings (F-04).
qt_add_executable(tst_recent tst_recent.cpp)
target_link_libraries(tst_recent PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_recent COMMAND tst_recent)
set_tests_properties(tst_recent PROPERTIES ENVIRONMENT "${NSL_OFFSCREEN_ENV}")
if(WIN32)
    set_target_properties(tst_recent PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

# The AppImage's self-setup (D-041): files, links and folders as on Linux.
if(LINUX)
    qt_add_executable(tst_linuxintegration tst_linuxintegration.cpp)
    target_link_libraries(tst_linuxintegration PRIVATE nsl_snap_core Qt6::Test)
    target_compile_definitions(tst_linuxintegration PRIVATE NSL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME tst_linuxintegration COMMAND tst_linuxintegration)
endif()
