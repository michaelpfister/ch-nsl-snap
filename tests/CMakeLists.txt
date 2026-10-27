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
    foreach(test_target tst_windowimage tst_editor tst_blur tst_view tst_capture tst_overlay tst_wincapture)
        set_target_properties(${test_target} PROPERTIES WIN32_EXECUTABLE FALSE)
    endforeach()
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
