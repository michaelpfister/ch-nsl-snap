qt_add_executable(tst_windowimage tst_windowimage.cpp)
target_link_libraries(tst_windowimage PRIVATE nsl_snap_core Qt6::Test)
add_test(NAME tst_windowimage COMMAND tst_windowimage)
