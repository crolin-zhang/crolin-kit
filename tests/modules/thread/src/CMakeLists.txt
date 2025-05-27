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

# 线程池自动动态调整功能测试
add_executable(thread_auto_adjust thread_auto_adjust.c)
target_link_libraries(thread_auto_adjust PRIVATE thread log)
target_include_directories(thread_auto_adjust PRIVATE ${CMAKE_BINARY_DIR}/include)
add_test(NAME ThreadAutoAdjustTest COMMAND thread_auto_adjust)

# 线程池调试测试
add_executable(thread_debug_test thread_debug_test.c)
target_link_libraries(thread_debug_test PRIVATE thread)
target_include_directories(thread_debug_test PRIVATE ${CMAKE_BINARY_DIR}/include)
add_test(NAME ThreadDebugTest COMMAND thread_debug_test)

# 线程池随机化测试
add_executable(thread_pool_test thread_pool_test.c)
target_link_libraries(thread_pool_test PRIVATE thread)
target_include_directories(thread_pool_test PRIVATE ${CMAKE_BINARY_DIR}/include)
add_test(NAME ThreadPoolRandomTest COMMAND thread_pool_test)
