add_executable(tests_flags flags_tests.cpp)
target_link_libraries(tests_flags Qt5::Core)
add_test(flagsTest ${CMAKE_BINARY_DIR}/bin/tests_flags)

add_executable(tests_flat_map flat_map_tests.cpp)
target_link_libraries(tests_flat_map Qt5::Core)
add_test(flatMapTest ${CMAKE_BINARY_DIR}/bin/tests_flat_map)

add_executable(tests_flat_set flat_set_tests.cpp)
target_link_libraries(tests_flat_set Qt5::Core)
add_test(flatSetTest ${CMAKE_BINARY_DIR}/bin/tests_flat_set)
