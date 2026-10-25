qt_add_executable(tst_windowimage tst_windowimage.cpp)
target_link_libraries(tst_windowimage PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_windowimage COMMAND tst_windowimage)

qt_add_executable(tst_editor tst_editor.cpp)
target_link_libraries(tst_editor PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_editor COMMAND tst_editor)
set_tests_properties(tst_editor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_blur tst_blur.cpp)
target_link_libraries(tst_blur PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_blur COMMAND tst_blur)

qt_add_executable(tst_view tst_view.cpp)
target_link_libraries(tst_view PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_view COMMAND tst_view)
set_tests_properties(tst_view PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
