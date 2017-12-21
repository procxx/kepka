add_executable(tests_flags flags_tests.cpp)
target_link_libraries(tests_flags Qt5::Core)
add_test(NAME flagsTest COMMAND tests_flags)

add_executable(tests_flat_map flat_map_tests.cpp)
target_link_libraries(tests_flat_map Qt5::Core)
add_test(NAME flatMapTest COMMAND tests_flat_map)

add_executable(tests_flat_set flat_set_tests.cpp)
target_link_libraries(tests_flat_set Qt5::Core)
add_test(NAME flatSetTest COMMAND tests_flat_set)
