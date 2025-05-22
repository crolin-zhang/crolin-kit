add_executable(thread_unit_test thread_unit_test.c)
target_link_libraries(thread_unit_test PRIVATE thread)
target_include_directories(thread_unit_test PRIVATE ${CMAKE_BINARY_DIR}/include)
add_test(NAME ThreadUnitTest COMMAND thread_unit_test)
