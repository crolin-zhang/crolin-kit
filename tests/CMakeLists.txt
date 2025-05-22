add_subdirectory(modules)

add_executable(test_thread_pool test_thread_pool.c)
target_link_libraries(test_thread_pool PRIVATE thread)
target_include_directories(test_thread_pool PRIVATE 
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core/thread/include
)

# 添加测试
add_test(NAME ThreadPoolTest COMMAND test_thread_pool)
