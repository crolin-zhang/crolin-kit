# 线程池基本功能示例
add_executable(thread_example thread_example.c)
target_link_libraries(thread_example PRIVATE thread)
target_include_directories(thread_example PRIVATE ${CMAKE_BINARY_DIR}/include)

# 线程池动态调整大小功能示例
add_executable(thread_resize_example thread_resize_example.c)
target_link_libraries(thread_resize_example PRIVATE thread)
target_include_directories(thread_resize_example PRIVATE ${CMAKE_BINARY_DIR}/include)

# 线程池自动动态调整功能示例
add_executable(thread_auto_adjust_example thread_auto_adjust_example.c)
target_link_libraries(thread_auto_adjust_example PRIVATE thread)
target_include_directories(thread_auto_adjust_example PRIVATE ${CMAKE_BINARY_DIR}/include)

# 线程池任务名称查询示例
add_executable(thread_task_names_example thread_task_names_example.c)
target_link_libraries(thread_task_names_example PRIVATE thread)
target_include_directories(thread_task_names_example PRIVATE ${CMAKE_BINARY_DIR}/include)

# 线程池综合示例
add_executable(thread_pool_example thread_pool_example.c)
target_link_libraries(thread_pool_example PRIVATE thread)
target_include_directories(thread_pool_example PRIVATE ${CMAKE_BINARY_DIR}/include)

# 线程池任务优先级示例
add_executable(thread_priority_example thread_priority_example.c)
target_link_libraries(thread_priority_example PRIVATE thread)
target_include_directories(thread_priority_example PRIVATE ${CMAKE_BINARY_DIR}/include)
