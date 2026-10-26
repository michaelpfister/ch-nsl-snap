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

# Captures real windows with Windows.Graphics.Capture: needs a desktop
# session, so it uses the normal "windows" platform, not offscreen.
if(WIN32)
    qt_add_executable(tst_wincapture tst_wincapture.cpp)
    target_link_libraries(tst_wincapture PRIVATE nsl_snap_core Qt6::Test)
    add_test(NAME tst_wincapture COMMAND tst_wincapture)
endif()
