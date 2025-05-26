# 线程池基本功能测试
add_executable(thread_unit_test thread_unit_test.c)
target_link_libraries(thread_unit_test PRIVATE thread)
target_include_directories(thread_unit_test PRIVATE ${CMAKE_BINARY_DIR}/include)
add_test(NAME ThreadUnitTest COMMAND thread_unit_test)

# 线程池动态调整大小功能测试
add_executable(thread_resize_test thread_resize_test.c)
target_link_libraries(thread_resize_test PRIVATE thread)
target_include_directories(thread_resize_test PRIVATE ${CMAKE_BINARY_DIR}/include)
add_test(NAME ThreadResizeTest COMMAND thread_resize_test)
