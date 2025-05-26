# 线程池基本功能示例
add_executable(thread_example thread_example.c)
target_link_libraries(thread_example PRIVATE thread)
target_include_directories(thread_example PRIVATE ${CMAKE_BINARY_DIR}/include)

# 线程池动态调整大小功能示例
add_executable(thread_resize_example thread_resize_example.c)
target_link_libraries(thread_resize_example PRIVATE thread)
target_include_directories(thread_resize_example PRIVATE ${CMAKE_BINARY_DIR}/include)
